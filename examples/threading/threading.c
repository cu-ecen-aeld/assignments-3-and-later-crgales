#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

typedef struct {
    int wait_to_obtain_ms;
    int wait_to_release_ms;
    pthread_mutex_t *mutex;
} thread_data_t;

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int rc;
    thread_data_t *thread_data = (thread_data_t*) thread_param;

    // wait
    usleep(1000 * thread_data->wait_to_obtain_ms);

    // obtain mutex
    rc = pthread_mutex_lock(thread_data->mutex);
    if (rc != 0) ERROR_LOG("pthread_mutex_lock error: %0d", rc);

    // wait
    usleep(1000 * thread_data->wait_to_release_ms);

    // release mutex
    rc = pthread_mutex_unlock(thread_data->mutex);
    if (rc != 0) ERROR_LOG("pthread_mutex_unlock error: %0d", rc);

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
    int rc;
    thread_data_t *thread_data;
    
    // Allocate memory for thread_data
    thread_data = malloc(sizeof(thread_data_t));

    if (thread_data == NULL) {
        ERROR_LOG("Unable to allocate memory for thread_data");
        return false;
    }

    // Setup mutex and wait arguments
    thread_data->mutex = mutex;
    thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data->wait_to_release_ms = wait_to_release_ms;

    DEBUG_LOG("creating thread");

    // Pass thread_data to created thread using threadfunc() as entry point
    rc = pthread_create(thread, NULL, threadfunc, thread_data);

    if (rc != 0) {
        ERROR_LOG("pthread_create returned %0d", rc);
        return false;
    } 
    
    DEBUG_LOG("pthread_create was successful");

    return true;
}

