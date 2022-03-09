// File:	worker.c

// List all group member's name: Akash Pathuri, Michael Elkhouri
// username of iLab: arp229, mre66
// iLab Server: ilab4.cs.rutgers.edu
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>

#include "worker.h"

#define STACK_SIZE SIGSTKSZ

int thread_count = 0;
int freeNode = 0; //remove if not used
int exitCalled = 0; //remove if not used

thread_queue *ready_queue[PRIORITY_LEVELS];
thread_queue *exit_queue;
thread_queue *waiting_queue;
thread_queue *join_queue;

thread_node *currentNode;
ucontext_t *scheduler_context;

int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // - after everything is set, push this thread into run queue and 
       // - make it ready for the execution.


	thread_count++;
	ucontext_t *context = (ucontext_t*)malloc(sizeof(ucontext_t));

	context->uc_link = NULL;
	context->uc_stack.ss_sp = malloc(STACK_SIZE);;
	context->uc_stack.ss_size = STACK_SIZE;
	context->uc_stack.ss_flags = 0;

	makecontext(context,(void *)&function, 1, arg);
	tcb *new_thread = (tcb*) malloc(sizeof(tcb));
	thread = thread_count;
	new_thread->id = thread;
	new_thread->context = context;
	new_thread->status = READY;
	new_thread->priority = 0;
	thread_node *new_thread_node = (thread_node*) malloc(sizeof(thread_node));
	new_thread_node->thread_tcb = new_thread;
	new_thread_node->next_thread = NULL;
	

	if(thread_count == 1){
		ucontext_t *main_context = (ucontext_t*)malloc(sizeof(ucontext_t));
		tcb *main_thread = (tcb*) malloc(sizeof(tcb));
		main_thread->id = 0;
		main_thread->context = getcontext(main_context);
		main_thread->status = RUNNING;
		main_thread->priority = 0;
		//initilize ready queue...........
		for(int x = 0; x<PRIORITY_LEVELS; x++){
			create_queue(ready_queue[x]);
		}
		thread_node *main_thread_node = (thread_node*) malloc(sizeof(thread_node));
		main_thread_node->thread_tcb = main_thread;
		main_thread_node->next_thread = NULL;
		currentNode = main_thread_node;
		enqueue(main_thread_node, ready_queue[0]);
		enqueue(new_thread_node, ready_queue[0]);

		create_schedule_context();
		schedule();
		//Later on in the project-----------------
	}else{
		enqueue(new_thread_node, ready_queue[0]);
	}

    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	//schedule();
	currentNode->thread_tcb->status = READY;
	swapcontext(currentNode->thread_tcb->context, scheduler_context);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	if (value_ptr != NULL) {
    	currentNode->thread_tcb->value_ptr = value_ptr;
  	}

	currentNode->thread_tcb->status = DONE;
	
	swapcontext(currentNode->thread_tcb->context, scheduler_context);

	//*/*/*/*/*/*/*/*/*/*/*/**/*/*/*/*/**/*/*/*/***/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*
	//figure out what this is ----------------------------------------------------------------------------------------
    //search joinQueue to see if thread that called join on currentThread exists there

	/*if (currentNode->thread_tcb->id == 0){
        exit(1);
    }
	
	if (searchJoin(currentNode->thread_tcb->id) == -1){
        dequeue(ready_queue[currentNode->thread_tcb->priority]);
        enqueue(currentNode, exit_queue);
    }
    else {
        dequeue(ready_queue[currentNode->thread_tcb->priority]);
        freeNode = 1;
    }

    currentNode->thread_tcb->value_ptr = value_ptr;
    exitCalled = 1;
    scheduler();*/


};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE

	//*/*/*/*/*/*/*/*/*/*/*/**/*/*/*/*/**/*/*/*/***/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*/*
	//figure out what this is ----------------------------------------------------------------------------------------
    
	if (searchExit(thread) == -1){
        currentNode->thread_tcb->joinID = &thread; 
        dequeue(ready_queue[currentNode->thread_tcb->priority]); 
        enqueue(currentNode, join_queue);
       // joinCalled = 1;
        //printf("adding thread to joinQueue: %d", currentNode->mythread->tid);
        //printf("thread parameter: %d saved parameter: %d\n", thread, currentNode->mythread->joinOn);
        scheduler();
    }



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


	// YOUR CODE HERE

// - schedule policy
#ifndef MLFQ
	// Choose RR
	sched_rr();
#else 
	// Choose MLFQ
	sched_mlfq();
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
void enqueue(thread_node * thread, thread_queue *queue){
	if(queue->first_node == NULL){
		queue->first_node = thread;
		queue->last_node = thread;
	}else{
		queue->last_node->next_thread = thread;
		queue->last_node = thread;
	}
	queue->size++;
	
}


thread_node *dequeue(thread_queue *queue){
	if(queue->first_node == NULL){
		return NULL;
	}
	thread_node *temp = queue->first_node;
	queue->first_node = queue->first_node->next_thread;
	temp->next_thread = NULL;
	if(queue->first_node == NULL){
		queue->last_node = NULL;
	}
	queue->size--;
	return temp;
}

void create_queue(thread_queue* queue) {
	queue = (thread_queue*) malloc(sizeof(thread_queue));
	queue->first_node = NULL;
	queue->last_node = NULL;
	queue->size = 0;
}

void create_schedule_context() {
	scheduler_context = (ucontext_t*) malloc(sizeof(ucontext_t));
	scheduler_context->uc_link = NULL; 
	scheduler_context->uc_stack.ss_sp = malloc(STACK_SIZE); 
	scheduler_context->uc_stack.ss_size = STACK_SIZE; 
	scheduler_context->uc_stack.ss_flags = 0;

	makecontext(scheduler_context, &schedule, 1, NULL);

}


