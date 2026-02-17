#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_data_t *data_ptr = (thread_data_t *)thread_param;   
    usleep(data_ptr->wait_to_obtain * 1000);
    int rc = pthread_mutex_lock(data_ptr->t_mutex);
    if (rc != 0) {
        ERROR_LOG("Failed to lock mutex");
        data_ptr->thread_complete_success = false;
        return thread_param;
    }
    usleep(data_ptr->wait_to_release * 1000);
    rc = pthread_mutex_unlock(data_ptr->t_mutex);
    if (rc != 0) {
        ERROR_LOG("Failed to unlock mutex");
        data_ptr->thread_complete_success = false;
        return thread_param;
    }
    data_ptr->thread_id = pthread_self();
    data_ptr->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    thread_data_t *data_ptr = (thread_data_t *)malloc(sizeof(thread_data_t));
    if (data_ptr == NULL) {
        ERROR_LOG("Failed to allocate memory");
        return false;
    }
    data_ptr->t_mutex = mutex;
    data_ptr->wait_to_obtain = wait_to_obtain_ms;
    data_ptr->wait_to_release = wait_to_release_ms;
    data_ptr->thread_complete_success = false;
    int rc = pthread_create(thread, NULL, threadfunc, (void*)data_ptr);
    if (rc != 0) {
        ERROR_LOG("Failed to create thread");
        free(data_ptr);
        return false;
    }
    return true;
}

