#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE
#define _XOPEN_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>

typedef uint mypthread_t;
typedef uint mymutex_t;

typedef enum State {
    READY, RUNNING,
    WAITING, BLOCKED,
    COMPLETED
} State;

typedef struct Queue Queue;

typedef struct threadControlBlock {
    /* add important states in a thread control block */
    // thread Id
    // thread status
    // thread context
    // thread stack
    // thread priority
    // And more ...
    mypthread_t thread_id;
    State state;
    ucontext_t context;
    void* return_values;

    long elapsed_time;
    mypthread_t waiting_on_thread_id;
    Queue* waiting_to_join_queue;
} tcb;

/* mutex struct definition */
typedef struct mypthread_mutex_t {
    mymutex_t mymutex_id;
    volatile int value;
    tcb* mutex_owner;
    Queue* access_queue;
} mypthread_mutex_t;

/* define your data structures here: */

/* Function Declarations: */

/* create a new thread */
int mypthread_create(mypthread_t* thread, pthread_attr_t* attr, void* (*function)(void*), void* arg);

/* give CPU possession to other user level threads voluntarily */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void* value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void** value_ptr);

/* initial the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr);

/* acquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t* mutex);

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t* mutex);

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t* mutex);

#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
