// File:	worker_t.h

// List all group member's name: Akash Pathuri, Michael Elkhouri
// username of iLab: arp229, mre66
// iLab Server: ilab4.cs.rutgers.edu

#ifndef WORKER_T_H
#define WORKER_T_H
#define PRIORITY_LEVELS 8
#define RUNNING 0
#define READY 1
#define BLOCKED 2
#define WAITING 3
#define STACK_SIZE SIGSTKSZ
#define INTERVAL 20



#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

typedef uint worker_t;

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...
	worker_t id;
	ucontext_t *context;
	worker_t join_on; 
	int priority;
	int status;
	int elapsed_counter;
	void *return_ptr; //stack pointer
	struct TCB *next_thread;
	//struct TCB *next_waiting_thread;
	//struct thread_queue *waiting_queue;
	/*
	mypthread_t thread_id;
    State state;
    ucontext_t context;
    void* return_values;

    long elapsed_time;
    mypthread_t waiting_on_thread_id;
    Queue* waiting_to_join_queue;
	*/

} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	// worker_mutex_t mutex_id;
    // volatile int value; //0 or 1, 0 being unlocked, 1 being locked
    // tcb* mutex_parent; 
    // thread_queue* queue;
} worker_mutex_t;


typedef struct thread_node {
	tcb *thread_tcb;
    struct thread_node *next_thread;
} thread_node;

typedef struct thread_queue{
	tcb *first_node;
	tcb *last_node;
	int size;
}thread_queue;


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

//helper functions
static void schedule();
static void sched_rr();
static void sched_mlfq();

thread_queue enqueue(tcb *thread, thread_queue *queue);
tcb *dequeue(thread_queue *queue);
void create_queue(thread_queue *queue);
void create_timer();
void signal_handler (int signum);
tcb *find_thread(worker_t thread);
tcb *search_queue(worker_t thread, thread_queue *queue);
void print_queue(thread_queue queue);

#endif
