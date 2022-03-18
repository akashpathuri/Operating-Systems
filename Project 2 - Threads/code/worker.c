// File:	worker.c

// List all group member's name: Akash Pathuri, Michael Elkhouri
// username of iLab: arp229, mre66
// iLab Server: ilab4.cs.rutgers.edu
#include "worker.h"

#define DEBUG 0

int thread_count = 0;
int thread_exiting = 0;
int thread_joining = 0;
struct itimerval it_val;

thread_queue ready_queue[PRIORITY_LEVELS];

tcb *current_node;
tcb *main_thread;
//ucontext_t *scheduler_context; //remove if not used

int worker_create(worker_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	// - create Thread Control Block (TCB)
	// - create and initialize the context of this worker thread
	// - allocate space of stack for this thread to run
	// - after everything is set, push this thread into run queue and 
	// - make it ready for the execution.
	if(thread_count == 0){
		ucontext_t main_context;
		//getcontext(&main_context);
		
		main_thread = (tcb*) malloc(sizeof(tcb));
		memset(main_thread, 0, sizeof(tcb));
		main_thread->id = 0;
		main_thread->context = &main_context;
		main_thread->status = RUNNING;
		main_thread->priority = 0;
		main_thread->next_thread = NULL;

		//initilize ready queue
		for(int x = 0; x<PRIORITY_LEVELS; x++){
			memset(&ready_queue[x], 0, sizeof(thread_queue));
			ready_queue[x].size = 0;
		}

		current_node = main_thread;
		ready_queue[0] = enqueue(main_thread, &ready_queue[0]);
	}

	ucontext_t *context = (ucontext_t*)malloc(sizeof(ucontext_t));
	getcontext(context);
	context->uc_link = NULL;
	context->uc_stack.ss_sp = malloc(STACK_SIZE);;
	context->uc_stack.ss_size = STACK_SIZE;
	context->uc_stack.ss_flags = 0;

	makecontext(context,(void *)function, 1, arg);
	tcb *new_thread = (tcb*) malloc(sizeof(tcb));
	memset(new_thread, 0, sizeof(tcb));
	*thread = ++thread_count;
	new_thread->id = thread_count;
	new_thread->context = context;
	new_thread->status = READY;
	new_thread->priority = 0;
	new_thread->next_thread = NULL;  
	ready_queue[0] = enqueue(new_thread, &ready_queue[0]);
	if(DEBUG)
		printf("Creating Thread: %d\n", ready_queue[0].last_node->id);
	
	
	if(thread_count == 1){
		getcontext(main_thread->context);
		if(thread_count == 1){
			create_timer();
			schedule();
		}
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
		//return;
        //exit(1);
    }else{
		if(DEBUG)
			printf("Exiting %d\n", current_node->id);
	
		if (value_ptr != NULL) {
			current_node->return_ptr = value_ptr;
		}
		
		// traverse the thread queue to find the current_node ID in any of the elements' waiting queue list of the tcb. 
		dequeue(&ready_queue[current_node->priority]);
		if(DEBUG)
			printf("Main Thread Waiting %d\n", main_thread->join_on);
		if(main_thread->join_on == current_node->id){
			if(DEBUG)
				printf("Main Thread Ready %d\n", main_thread->join_on);
			main_thread->status = READY;
		}

		// if(current_node->waiting_queue !=NULL){
		// 	tcb * search_node = current_node->waiting_queue->first_node; 
		// 	while(search_node != NULL){
		// 		if(search_node->join_on == current_node->id)
		// 			search_node->status = READY;
		// 		tcb * next_node = search_node->next_waiting_thread;
		// 		search_node->next_waiting_thread = NULL;
		// 		search_node = next_node;
		// 	}
		// 	free(current_node->waiting_queue);
		// }
		free(current_node->context->uc_stack.ss_sp);
		free(current_node);
		thread_exiting = 1;
		schedule();
	}
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	// if (thread > thread_count){ //remove if not used
	// 	return ESRCH;
	// }
	if(DEBUG)
		printf("Joining %d with %d\n", current_node->id, thread);


	
	tcb *wait_on = find_thread(thread);
	
	if(wait_on == NULL){
		if(DEBUG)
			printf("Thread Found %d\n", thread);
		return 0;
	}else{
		if(DEBUG)
			printf("Thread Not Found %d\n", thread);
	}
	current_node->join_on = thread;
	current_node->status = WAITING;	
	//enqueueWait(current_node, wait_on->waiting_queue);
	thread_joining = 1;
	schedule();
	if(DEBUG) 
		printf("Returning Join Thread %d\n", thread);	
	
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex
	// YOUR CODE HERE
//	mutex = (worker_mutex_t*)malloc(sizeof(worker_mutex_t));
  /*
    mutex->mymutex_id = (*mutex);
    mutex->value = 0;
	mutex->mutex_owner = NULL;
	mutex->queue = (thread_queue*) malloc(sizeof(thread_queue));
	memset(mutex->queue, 0, sizeof(thread_queue));
	mutex->queue->first_node = NULL;
	mutex->queue->last_node = NULL;
	mutex->queue->size = 0;
*/
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread
        /*
		if(__sync_lock_test_and_set(&mutex->value, 1) != 0){
			enqueue(currentNode, mutex->queue);
            currentNode->status = BLOCKED;
			schedule();
           // swapcontext(&thread->context, &scheduler_thread->context);
        }
        else {
             mutex->mutex_parent = currentNode->id;
        }
		
        */
		// YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	/*
     if (mutex->mutex_parent == currentNode->id) {
	       mutex->mutex_parent = mutex->queue->first_node->id;
	       dequeue(mutex->queue);
	       if (mutex->mutex_parent != NULL)
	          mutex->mutex_parent->status = READY;
	}

	*/

	// YOUR CODE HERE
	return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
    /*
	thread_queue* queue = mutex->queue;
       if (queue == NULL)
        return;
    tcb* curr = queue->first_node;
    while (curr != NULL) {
        tcb* next = curr->next_thread;
        free(current);
        current = next;
    }
	free(mutex);


	*/
	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (RR or MLFQ)



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
	if (thread_joining){
		thread_joining = 0;
		if(DEBUG)
			print_queue(ready_queue[level]);
		while(ready_queue[level].first_node->status != READY){
			ready_queue[level] = enqueue(dequeue(&ready_queue[level]), &ready_queue[level]);
		}
		current_node = ready_queue[level].first_node;
		current_node->status = RUNNING;
		if(DEBUG)
			print_queue(ready_queue[level]);
		swapcontext(main_thread->context, current_node->context);
	}else if (thread_exiting){ 
		thread_exiting = 0;
		if(DEBUG)
			print_queue(ready_queue[level]);
		while(ready_queue[level].first_node->status != READY){
			ready_queue[level] = enqueue(dequeue(&ready_queue[level]), &ready_queue[level]);
		}
		current_node = ready_queue[level].first_node;
		current_node->status = RUNNING;
		if(DEBUG)
			print_queue(ready_queue[level]);
		setcontext(current_node->context);
	}else if(ready_queue[level].size>1){
		tcb *running_thread = dequeue(&ready_queue[level]);
		if(running_thread->status == RUNNING)
			running_thread->status = READY;
		ready_queue[level] = enqueue(running_thread, &ready_queue[level]);
	
		while(ready_queue[level].first_node->status != READY){
			ready_queue[level] = enqueue(dequeue(&ready_queue[level]), &ready_queue[level]);
		}
		current_node = ready_queue[level].first_node;
		current_node->status = RUNNING;
		swapcontext(running_thread->context, current_node->context);
	}else{
		if(DEBUG){
			print_queue(ready_queue[level]);
			printf("Scheduling last thread (main): %d\n", ready_queue[level].size);
		}
	}
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq(int level) {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

	/*
		Rule 1: If Priority(A) > Priority(B), A runs (B doesn’t).
		Rule 2: If Priority(A) = Priority(B), A & B run in RR.
		Rule 3: When a job enters the system, it is placed at the highest priority (the topmost queue).
		Rule 4a: If a job uses up an entire time slice while running, its priority is reduced (i.e., it moves down one queue).
		Rule 4b: If a job gives up the CPU before the time slice is up, it stays at the same priority level.
		Rule 5: After some time period S, move all the jobs in the system to the topmost queue
	*/




	
}

// Feel free to add any other functions you need
thread_queue enqueue(tcb * thread, thread_queue *queue){
	if(queue == NULL)
		create_queue(queue);

	if(queue->size == 0){
		queue->first_node = thread;
		queue->last_node = queue->first_node;
	}else{
		queue->last_node->next_thread = thread;
		queue->last_node = queue->last_node->next_thread;
	}
	queue->size = queue->size + 1;
	if (DEBUG==1){
		printf("first:%d, last:%d\t", queue->first_node->id, queue->last_node->id);
		print_queue(*queue);
	}

	return *queue;
}


void enqueueWait(tcb * thread, thread_queue *queue){
	if(queue == NULL)
		create_queue(queue);
	if(DEBUG) 
		printf("Making queue with %d\n", thread->id);
	if(queue->first_node == NULL){
		queue->first_node = thread;
		queue->last_node = thread;
	}else{
		queue->last_node->next_waiting_thread = thread;
		queue->last_node = thread;
	}
	queue->size++;
	if(DEBUG){
		printf("Past creation of wait queue or addition of element\n");
		print_queue(*queue);
	}
	
}

tcb *dequeue(thread_queue *queue){
	if(queue->first_node == NULL){
		return NULL;
	}
	tcb *temp = queue->first_node;
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
	memset(queue, 0, sizeof(thread_queue));
	queue->first_node = NULL;
	queue->last_node = NULL;
	queue->size = 0;
}

void create_schedule_context() {
	// scheduler_context = (ucontext_t*) malloc(sizeof(ucontext_t));
	// scheduler_context->uc_link = NULL;
	// scheduler_context->uc_stack.ss_sp = malloc(STACK_SIZE);
	// scheduler_context->uc_stack.ss_size = STACK_SIZE;
	// scheduler_context->uc_stack.ss_flags = 0;

	// makecontext(scheduler_context, &schedule, 1, NULL);
}

void create_timer(){
	if (signal(SIGALRM, (void (*)(int)) signal_handler) == SIG_ERR) {
		printf("Unable to catch SIGALRM\n");
		exit(1);
  	}

	it_val.it_value.tv_sec =     INTERVAL/1000;
  	it_val.it_value.tv_usec =    (INTERVAL*1000) % 1000000;	
  	it_val.it_interval = it_val.it_value;

	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		printf("error calling setitimer()\n");
		exit(1);
	}
	//printf("Timer Created\n");
}

void signal_handler (int signum){
	if(DEBUG==1)
		printf("Timer Interrupt %d\n", ready_queue[0].size);
    schedule();
}

tcb *find_thread(worker_t thread){
	for(int x = 0; x<PRIORITY_LEVELS; x++){
		if(DEBUG == 2 )
			printf("Searching %d Level\n", x);
		tcb *search_node = search_queue(thread, &ready_queue[x]);
		if(search_node != NULL){
			return search_node;
		}
	}
	return NULL;
}

tcb *search_queue(worker_t thread, thread_queue *queue){
	tcb * search_node = queue->first_node; 
	
	while(search_node != NULL){
		if(DEBUG == 2)
			printf("Searching %d Node\t", search_node->id);
		if(search_node->id == thread){
			if(DEBUG == 2)
				printf("\n");
			return search_node;
		}
		search_node = search_node->next_thread;
	}
	if(DEBUG == 2)
			printf("\n");
	return NULL;
}

void print_queue(thread_queue queue){
	tcb *search_node = queue.first_node;

	while(search_node != NULL){
		printf("%d(%d) -> ",search_node->id, search_node->status);
		search_node = search_node->next_thread;
	}
	printf("end   size:%d\n",queue.size);
}
