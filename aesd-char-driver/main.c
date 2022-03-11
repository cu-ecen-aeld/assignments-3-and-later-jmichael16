/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes, Jake Michael
 * @date Mar-03-2022
 * @copyright Copyright (c) 2019
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>        // file_operations
#include <linux/slab.h>      // for kmalloc/kfree
#include <linux/string.h>    // for string handling 
#include <linux/uaccess.h>  // for access_ok

// device driver dependencies:
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Jake Michael"); 
MODULE_LICENSE("Dual BSD/GPL");

// define global, persistent data structure for the aesd char device
struct aesd_dev aesd_device;

// the file operations which are supported by the driver
// all unlisted are NULL and so are unsupported
struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

// this is the first operation performed on the device file
int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
  
  // used Linux Device Drivers scull device as reference:
  // declare dummy pointer to aesd_dev struct
  struct aesd_dev *dev;

  // populate dev with a pointer to the aesd_dev struct associated with inode
  dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

  // set private_data to the aesd_device struct
  filp->private_data = dev;

	return 0;
}

// invoked when the file structure is being released 
int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
  // nothing to do here? nothing needs to be freed?
	return 0;
}

// used to retrieve data from the device
// a non-negative return value represents the number of bytes successfully read
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
  ssize_t read_offset = 0;
  ssize_t read_size = 0;
  ssize_t uncopied_count = 0;
  struct aesd_buffer_entry *read_entry = NULL;

	PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

  // error check inputs - don't want a kernel seg-fault
  if (count == 0) { return 0; }
  if (filp == NULL || buf == NULL || f_pos == NULL) { return -EFAULT; }

  struct aesd_dev *dev = (struct aesd_dev *) filp->private_data;

  if( mutex_lock_interruptible( &(dev->lock) ) )
  {
    PDEBUG(KERN_ERR "aesd_read: could not acquire lock");
    return -ERESTARTSYS;
  }

  read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->cbuf), 
                                                               *f_pos, 
                                                               &read_offset);
  if ( !read_entry )
  {
    goto handle_errors;
  } 
  else 
  {
    // plan to read only up to end of entry if count is large enough,
    // else we only read count bytes
    read_size = read_entry->size - read_offset;
    if (read_size > count) 
    {
      read_size = count; // only read what user requests
    }
  }

  // check accessibility of userspace buffer
  if ( !access_ok(VERIFY_WRITE, buf, count) )
  {
    PDEBUG(KERN_ERR "aesd_read: access_ok fail");
    retval = -EFAULT;
    goto handle_errors;
  }
  uncopied_count = copy_to_user(buf, read_entry->buffptr + read_offset, read_size); 
  retval = read_size - uncopied_count;
  *fpos += retval;

handle_errors:
  mutex_unlock( &(dev->lock) );
	return retval;
}

// sends data to the device
// a non-negative return value represents the number of bytes successfully written
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
  ssize_t uncopied_count = 0;
  ssize_t retval = -ENOMEM;
  const char* overwritten = NULL; 

	PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

  // error check inputs - don't want a kernel seg-fault
  if (count == 0) { return 0; }
  if (filp == NULL || buf == NULL || f_pos == NULL) { return -EFAULT; }

  // dereference private_data from file pointer
  struct aesd_dev *dev = (struct aesd_dev *) filp->private_data;

  // acquire the mutex lock
  if( mutex_lock_interruptible( &(dev->lock) ) )
  {
    PDEBUG(KERN_ERR "aesd_write: could not acquire lock");
    return -ERESTARTSYS;
  }

  if (dev->write_append.size == 0)
  {
    // kmalloc a buffer of count size for write_append entry 
    // assumes size is zero initially
    dev->write_append.buffptr = kmalloc(count*sizeof(char), GFP_KERNEL);
    if (dev->write_append.buffptr == NULL) 
    {
      PDEBUG(KERN_ERR "aesd_write: kmalloc fail");
      goto handle_errors;
    }
  }
  else 
  {
    // need to realloc the buffer to existing size + count bytes
    dev->write_append.buffptr = krealloc(dev->write_append.buffptr, 
                                         (dev->write_append.size + count)*sizeof(char), 
                                         GFP_KERNEL); 
    if (dev->write_append.buffptr == NULL) 
    {
      PDEBUG(KERN_ERR "aesd_write: krealloc fail");
      goto handle_errors;
    }
  }

  // check accessibility of userspace buffer
  if ( !access_ok(VERIFY_READ, buf, count) )
  {
    PDEBUG(KERN_ERR "aesd_write: access_ok fail");
    retval = -EFAULT;
    goto handle_errors;
  }

  // copy buffer from user to heap allocated entry
  uncopied_count = copy_from_user(dev->write_append.buffptr + dev->write_append.size, 
                                  buf, 
                                  count);

  // update the return value and entry size with the number of bytes actually copied
  retval = count - uncopied_count;
  dev->write_append.size += retval;

  if( !strchr(buf, '\n') ) 
  {
    // store heap allocated buffer in circular buffer, make sure to free if non-NULL return
    overwritten = aesd_circular_buffer_add_entry(&(dev->cbuf), &(dev->write_append));
    if ( !overwritten ) 
    {
      kfree(overwritten);
    }
  }

  // handle error returns
  handle_errors:
    mutex_unlock( &(dev->lock) );

	return retval;
}

// sets up the aesd_dev 
static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);
	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	// initialize the AESD specific portion of the device
	mutex_init(&(aesd_device.lock);

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */

	unregister_chrdev_region(devno, 1);
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
