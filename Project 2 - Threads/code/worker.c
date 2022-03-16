// File:	worker.c

// List all group member's name: Akash Pathuri, Michael Elkhouri
// username of iLab: arp229, mre66
// iLab Server: ilab4.cs.rutgers.edu
#include "worker.h"


int thread_count = 0;
int freeNode = 0; //remove if not used
int exitCalled = 0; //remove if not used

struct itimerval it_val;

thread_queue ready_queue[PRIORITY_LEVELS];

tcb *current_node;
ucontext_t *scheduler_context; //remove if not used

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
	memset(new_thread, 0, sizeof(tcb));
	thread = &(thread_count);
	new_thread->id = *thread;
	new_thread->context = context;
	new_thread->status = READY;
	new_thread->priority = 0;
	new_thread->next_thread = NULL;  
	/*thread_node *new_thread_node = (thread_node*) malloc(sizeof(thread_node));
	new_thread_node->thread_tcb = new_thread;
	new_thread_node->next_thread = NULL;*/  //remove if not used
	

	if(thread_count == 1){
		ucontext_t *main_context = (ucontext_t*)malloc(sizeof(ucontext_t));
		tcb *main_thread = (tcb*) malloc(sizeof(tcb));
		memset(main_thread, 0, sizeof(tcb));
		main_thread->id = 0;
		main_thread->context = main_context;
		main_thread->status = RUNNING;
		main_thread->priority = 0;
		main_thread->next_thread = NULL;
		//initilize ready queue...........
		
		for(int x = 0; x<PRIORITY_LEVELS; x++){
			//create_queue(ready_queue[x]);
			ready_queue[x].size = 0;
		}


		/*thread_node *main_thread_node = (thread_node*) malloc(sizeof(thread_node));
		main_thread_node->thread_tcb = main_thread;
		main_thread_node->next_thread = NULL;*/ //remove if not used
		current_node = main_thread;
		enqueue(main_thread, ready_queue[0]);
		enqueue(new_thread, ready_queue[0]);


		//create_schedule_context();
		create_timer();
		schedule();
	}else{
		enqueue(new_thread, ready_queue[0]);
	}
		printf("Thread %ls \n", thread);

    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	//schedule();
	//current_node->status = READY;
	//enqueue(dequeue(ready_queue[current_node->priority]), ready_queue[current_node->priority]);
	schedule();
	//swapcontext(current_node->context, current_node->next_thread->context);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	if (current_node->id == 0){
        printf("parent called exit\n");
        exit(1);
    }

	if (value_ptr != NULL) {
    	current_node->return_ptr = value_ptr;
  	}
	
	// traverse the thread queue to find the current_node ID in any of the elements' waiting queue list of the tcb. 
	
	dequeue(ready_queue[current_node->priority]);
	if(current_node->waiting_queue !=NULL){
		tcb * search_node = current_node->waiting_queue->first_node; 
		while(search_node != NULL){
			if(search_node->join_on == current_node->id)
				search_node->status = READY;
			tcb * next_node = search_node->next_waiting_thread;
			search_node->next_waiting_thread = NULL;
			search_node = next_node;
		}
		free(current_node->waiting_queue);
	}
	free(current_node->context->uc_stack.ss_sp);
	free(current_node);
	schedule();


};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	// if (thread > thread_count){ //remove if not used
	// 	return ESRCH;
	// }
	current_node->status = WAITING;
	tcb *wait_on = find_thread(thread);
	current_node->join_on = thread;
	printf("testing\n");
	enqueueWait(current_node, wait_on->waiting_queue);
	schedule();

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
	//swapcontext(ready_queue[0]->first_node->context);


	// YOUR CODE HERE
	sched_rr(0);
	// - schedule policy
//	#ifndef MLFQ
		// Choose RR
		
//	#else 
		// Choose MLFQ
//		sched_mlfq();
//	#endif

}

/* Round-robin (RR) scheduling algorithm */
static void sched_rr(int level) {
	// - your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	if(ready_queue[level].size>1 && current_node->id == ready_queue[level].first_node->id){
		tcb *running_thread = dequeue(ready_queue[level]);
		running_thread->status = READY;
		enqueue(running_thread, ready_queue[level]);
		while(ready_queue[level].first_node->status != READY){
			enqueue(dequeue(ready_queue[level]), ready_queue[level]);
		}
		current_node = ready_queue[level].first_node;
		current_node->status = RUNNING;
		swapcontext(running_thread->context, current_node->context);
	}
	//ready_queue[0]->first_node->thread_tcb.


}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

// Feel free to add any other functions you need
void enqueue(tcb * thread, thread_queue queue){
	//if(queue == NULL)
		//create_queue(queue);

	if(queue.size == 0){
		queue.first_node = thread;
		queue.last_node = thread;
	}else{
		queue.last_node->next_thread = thread;
		queue.last_node = thread;
	}
	queue.size++;
	
}


void enqueueWait(tcb * thread, thread_queue *queue){
	if(queue == NULL)
		create_queue(queue);

	if(queue->first_node == NULL){
		queue->first_node = thread;
		queue->last_node = thread;
	}else{
		queue->last_node->next_waiting_thread = thread;
		queue->last_node = thread;
	}
	queue->size++;
	
}

tcb *dequeue(thread_queue queue){
	if(queue.first_node == NULL){
		return NULL;
	}
	tcb *temp = queue.first_node;
	queue.first_node = queue.first_node->next_thread;
	temp->next_thread = NULL;
	if(queue.first_node == NULL){
		queue.last_node = NULL;
	}
	queue.size--;
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

void create_timer(){
	if (signal(SIGALRM, (void (*)(int)) signal_handler) == SIG_ERR) {
		printf("Unable to catch SIGALRM");
		exit(1);
  	}

	it_val.it_value.tv_sec =     INTERVAL/1000;
  	it_val.it_value.tv_usec =    (INTERVAL*1000) % 1000000;	
  	it_val.it_interval = it_val.it_value;

	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		printf("error calling setitimer()");
		exit(1);
	}
	printf("Timer Created\n");
}

void signal_handler (int signum){
	printf("Scheduling\n");
    schedule();
}

tcb *find_thread(worker_t thread){
	for(int x = 0; x<PRIORITY_LEVELS; x++){
		tcb *search_node = search_queue(thread, ready_queue[x]);
		if(search_node != NULL){
			return search_node;
		}
	}
	return NULL;
}

tcb *search_queue(worker_t thread, thread_queue queue){
	tcb * search_node = queue.first_node; 

	while(search_node != NULL){
		if(search_node->id == thread){
			return search_node;
		}
		search_node = search_node->next_thread;
	}
	return NULL;
}
