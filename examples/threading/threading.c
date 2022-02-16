/* ----------------------------------------------------------------------------
 * @file threading.c
 * @brief A threading example  
 * @author Jake Michael, jami1063@colorado.edu
 *         Dan Walkes
 *---------------------------------------------------------------------------*/

#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
  // return codes / error handling
  int rc;
  int completion_error = false;

  // retrieve thread_parameters by dereferencing data structure
  // note that we cast the void* into a struct thread_data* 
  struct thread_data* tdata = (struct thread_data *) thread_param;

  // wait to obtain mutex
  rc = usleep(tdata->wait_to_obtain_ms*1000);
  if (rc != 0) {
    ERROR_LOG("usleep failed, returned %s\n", strerror(rc));  
    completion_error = true;
  }

  // obtain mutex
  rc = pthread_mutex_lock(tdata->mutex_pass_to_thread);
  if (rc != 0) {
    ERROR_LOG("pthread_mutex_lock failed, returned %s\n", strerror(rc));  
    completion_error = true;
  }

  // wait to release mutex
  rc = usleep(tdata->wait_to_release_ms*1000);
  if (rc != 0) {
    ERROR_LOG("usleep failed, returned %s\n", strerror(rc));  
    completion_error = true;
  }
  
  // release mutex
  rc = pthread_mutex_unlock(tdata->mutex_pass_to_thread);
  if (rc != 0) {
    ERROR_LOG("pthread_mutex_unlock failed, returned %s\n", strerror(rc));  
    completion_error = true;
  }

  if (completion_error == false) {
    tdata->thread_complete_success = true;
  }

  return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
  int rc;

  // error check arguments from the caller
  if (wait_to_obtain_ms < 0 || wait_to_release_ms < 0 || thread == NULL || mutex == NULL) {
    return false;
  }

  // malloc the thread_param parameters which we will pass to the thread
  // and dereference inside the thread start function. note that the caller
  // is responsible for freeing this memory.
  struct thread_data* thread_param = (struct thread_data*) malloc(sizeof(struct thread_data));
  if (thread_param == NULL) {
    ERROR_LOG("malloc fail for thread_param, returned NULL\n");
    return false;
  }

  // populate the thread_param datastructure
  thread_param->mutex_pass_to_thread = mutex;
  thread_param->wait_to_obtain_ms = (unsigned int) wait_to_obtain_ms;
  thread_param->wait_to_release_ms = (unsigned int) wait_to_release_ms;
  thread_param->thread_complete_success = false;

  // create the pthread with malloc'd thread_data
  rc = pthread_create(thread, NULL, threadfunc, (void*) thread_param);
  if (rc != 0) {
    ERROR_LOG("pthread_create fail, returned %s\n", strerror(rc));
    free(thread_param); // Is this needed?
    return false;
  }

  return true;
}

