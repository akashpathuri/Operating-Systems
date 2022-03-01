// File:	worker.c

// List all group member's name: Akash Pathuri
// username of iLab:
// iLab Server:
#include <ucontext.h>
#include <signal.h>

#include "worker.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
#define STACK_SIZE SIGSTKSZ

int threadCount = 0;

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.
	if(threadCount == 0){
		ucontext_t masterCXT;
		tcb *masterThread = (tcb*) malloc(sizeof(tcb));
		masterThread->id = threadCount;
		masterThread->cxt = masterCXT;

	}

	threadCount++;
	ucontext_t cxt;

	cxt.uc_link=NULL;
	cxt.uc_stack.ss_sp = malloc(STACK_SIZE);;
	cxt.uc_stack.ss_size=STACK_SIZE;
	cxt.uc_stack.ss_flags=0;

	makecontext(&cxt,(void *)&function, 1, arg);
	tcb *thread = (tcb*) malloc(sizeof(tcb));
	thread->id = threadCount;
	
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (RR or MLFQ)

	// if (sched == RR)
	//		sched_rr();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// - schedule policy
#ifndef MLFQ
	// Choose RR
#else 
	// Choose MLFQ
#endif

}

/* Round-robin (RR) scheduling algorithm */
static void sched_rr() {
	// - your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

// Feel free to add any other functions you need

// YOUR CODE HERE

