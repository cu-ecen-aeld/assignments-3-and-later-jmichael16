#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo, Jake Michael
# Resources: Mastering Embedded Linux Programming, Simmons, 2015

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # Kernel build steps 
    echo "starting kernel build steps:"
    echo "1 - deep clean the kernel build tree, remove any existing config"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "2 - configure for virt arm dev board to simulate"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "3 - build the kernal image for booting with QEMU"
    make -j 4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "4 - build modules and device tree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

fi 

echo "Adding the Image in outdir"
# force copy if file already present
cp -f ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create necessary base directories
echo "creating the base directories:"
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir bin dev etc home lib proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # Configure busybox
    echo "configuring busybox"
    make distclean
    make defconfig
else
    cd busybox
fi

# the following busybox steps follow Mastering Embedded Linux Programming as a guide
# Make and install busybox
echo "making and installing busybox"
make -j 4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
sudo make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "returning to rootfs"
cd ${OUTDIR}/rootfs

echo "library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# Add library dependencies to rootfs
echo "add library dependencies to rootfs"
SYSROOT=$("${CROSS_COMPILE}gcc -print-sysroot")
cp -a $SYSROOT/lib/ld-linux-armhf.so.3 lib
cp -a $SYSROOT/lib/ld-2.19.so lib
cp -a $SYSROOT/lib/libc.so.6 lib
cp -a $SYSROOT/lib/libc-2.19.so lib
cp -a $SYSROOT/lib/libm.so.6 lib
cp -a $SYSROOT/lib/libm-2.19.so lib

# Make device nodes
echo "make device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# Clean and build the writer utility
echo "clean and build writer utility"
cd $FINDER_APP_DIR
make clean && make CROSS_COMPILE=${CROSS_COMPILE}

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "copying finder scripts and executables to /home directory on rootfs"
cp -r $FINDER_APP_DIR/conf/ $OUTDIR/rootfs/home # includes username.txt and assignment.txt
cp $FINDER_APP_DIR/finder.sh $OUTDIR/rootfs/home/
cp $FINDER_APP_DIR/finder-test.sh $OUTDIR/rootfs/home/
cp $FINDER_APP_DIR/writer $OUTDIR/rootfs/home/

# Chown the root directory
echo "chowning the rootfs directory"
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# Create initramfs.cpio.gz
echo "creating initramfs.cpio.gz"
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ${OUTDIR}
# force override if file already exists
gzip -f initramfs.cpio

