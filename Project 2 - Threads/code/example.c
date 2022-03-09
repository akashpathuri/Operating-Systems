#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <ucontext.h>
#include <errno.h>
//#include <pthread.h>
#include "example.h"

int maxThreads = 500;

//bunch of flags
int isFirstThread = 0;
int addedParent = 0;
int threadCount = 1;
int exitCalled = 0;
int joinCalled = 0;
int freeNode = 0;
int setWaiting = 0;

//for setting timers
struct itimerval it_val;
long int scheduleCount = 0;

//currently executing node/thread
threadNode *currentNode = NULL; 

//various queues for scheduler
ready_queue *readyQueue;
join_queue *joinQueue;
exit_queue *exitQueue;


int my_pthread_create( my_pthread_t * thread, my_pthread_attr_t * attr, void *(*function)(void*), void * arg){
    	
    	if (threadCount == 500){
		return EAGAIN;
	}

    //check if first time this function has been called
    if (isFirstThread == 0){
        //printf("first thread routine\n");
        isFirstThread = 1;

        //save context of current thread (calling function)
        ucontext_t *parent, *child = NULL;
        parent = (ucontext_t*)malloc(sizeof(ucontext_t));
        child = (ucontext_t*)malloc(sizeof(ucontext_t));

        if (getcontext(parent) == -1){
            perror("getcontext: could not get parent context\n");
            exit(1);
        }
        //if added parent to queue
        if (addedParent == 0){

            //printf("adding parent context and creating child context\n");
            //get context that will used to create new context
            if (getcontext(child) == -1){
                perror("getContext: could not get child context");
                exit(1);
            }

            //modify context for a new stack
            //printf("modifying child stack\n");
            child->uc_link = 0;
            child->uc_stack.ss_sp = malloc(STACK_SIZE);
            child->uc_stack.ss_size = STACK_SIZE;
            child->uc_stack.ss_flags = 0;

            if(child->uc_stack.ss_sp == 0){
                perror("malloc: could not allocate stack");
            }

            //printf("making child context\n");

            //link new context with a function
            makecontext(child, (void*)function, 1, arg);
            
            //create my_pthread__t object for child thread
            my_pthread_t * childThread = (my_pthread_t*)malloc(sizeof(my_pthread_t));
            childThread->tid = threadCount;
            childThread->context = child;
            thread->tid = childThread->tid;
            //printf("create: tid = %d\n", thread->tid);
            //thread = (pthread_t*)malloc(sizeof(pthread_t));
            //childThread->pthread = thread;
            childThread->exited = 0;
            threadNode * childNode = (threadNode *)malloc(sizeof(threadNode));
            childNode->mythread = childThread;
            childNode->next = NULL;

            //create my_pthread_t object for parent thread
            my_pthread_t * parentThread = (my_pthread_t*)malloc(sizeof(my_pthread_t));
            parentThread->tid = 0;
            parentThread->context = parent;
            threadNode * parentNode = (threadNode *)malloc(sizeof(threadNode));
            parentNode->mythread = parentThread;
            parentNode->next = NULL;

            
            initializeScheduler();

            //printf("adding parent and child to ready queue\n");
            currentNode = parentNode;
            //printf("creating thread: %d\n", parentNode->mythread->tid);
            enqueueReady(0, parentNode);
            //printf("creating thread: %d\n", childNode->mythread->tid);
            enqueueReady(0, childNode);
            addedParent = 1;

            //run scheduler for the first time
            scheduler();
        }
        return 0;
    }
    else {

        //sets up a new thread and adds it to ready queue
        ucontext_t *child = (ucontext_t*)malloc(sizeof(ucontext_t));

        if (getcontext(child) == -1){
                perror("getContext: could not get context");
                return -1;
            }

            //modify context for a new stack
            child->uc_link = 0;
            child->uc_stack.ss_sp = malloc(STACK_SIZE);
            child->uc_stack.ss_size = STACK_SIZE;
            child->uc_stack.ss_flags = 0;

            if(child->uc_stack.ss_sp == 0){
                perror("malloc: could not allocate stack");
                return -1;
            }

            threadCount++;

            //create the context for child
            makecontext(child, (void*)function, 1, arg);

            //create my_pthread_t for child thread
            my_pthread_t * childThread = (my_pthread_t*)malloc(sizeof(my_pthread_t));
            childThread->tid = threadCount;
            childThread->context = child;
            thread->tid = childThread->tid;
            //printf("create: tid = %d\n", thread->tid);
            //thread = (pthread_t*)malloc(sizeof(pthread_t));
            //childThread->pthread = thread;
            childThread->exited = 0;
            threadNode * childNode = (threadNode *)malloc(sizeof(threadNode));
            childNode->mythread = childThread;
            childNode->next = NULL;
            //printf("creating thread: %d\n", childNode->mythread->tid);
            enqueueReady(0, childNode);
            return 0;
    }
}

void my_pthread_yield(){
    //reset timer
    it_val.it_value.tv_sec = 0;
    it_val.it_value.tv_usec = 0;
    it_val.it_interval = it_val.it_value;

    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1){
        perror("error calling setitimer()");
        exit(1);
    }
    scheduler();
}

/* 
calling thread checks to see if there are any threads waiting on it. If so
the waiting threads are rescheduled. If there are not any waiting threads,
we ass the calling thread to a list of exited threads.
*/
void my_pthread_exit(void *value_ptr){
    //printf("my_pthread_exit called \n");

    if (currentNode->mythread->tid == 0){
        //printf("parent called exit\n");
        exit(1);
    }

    //reset timer
    it_val.it_value.tv_sec = 0;
    it_val.it_value.tv_usec = 0;
    it_val.it_interval = it_val.it_value;

    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1){
        perror("error calling setitimer()");
        exit(1);
    }

    //search joinQueue to see if thread that called join on currentThread exists there
    if (searchJoin(currentNode->mythread->tid) == -1){
        dequeueReady(currentNode->level);
        enqueueExit(currentNode);
    }
    else {
        dequeueReady(currentNode->level);
        freeNode = 1;
    }

    currentNode->mythread->value_ptr = value_ptr;
    exitCalled = 1;
    scheduler();
}

/*
calling thread checks to see if there the specified thread has been exited. does
this by searching through exited list. If not, we add the calling thread to the
join list (queue?)
*/

int my_pthread_join(my_pthread_t thread, void **value_ptr){
	
	if (thread.tid > threadCount){
		return ESRCH;
	}

    //printf("my_pthread_join called\n");

    //reset timer
    it_val.it_value.tv_sec = 0;
    it_val.it_value.tv_usec = 0;
    it_val.it_interval = it_val.it_value;

    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1){
        perror("error calling setitimer()");
        exit(1);
    }

    //search exitQueue to see if specified thread has already been exited

    if (searchExit(thread.tid) == -1){
        currentNode->mythread->joinOn = thread.tid;
        dequeueReady(currentNode->level);
        enqueueJoin(currentNode);
        joinCalled = 1;
        //printf("adding thread to joinQueue: %d", currentNode->mythread->tid);
        //printf("thread parameter: %d saved parameter: %d\n", thread, currentNode->mythread->joinOn);
        scheduler();
    }

    return 0;
}


int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const my_pthread_mutexattr_t *mutexattr){
	
	if (mutex->init == 1){
		return EBUSY;
	}	

	mutex->init = 1;
    mutex->lock = UNLOCKED;
    mutex->queueLock = UNLOCKED;
    mutex->waitQueue = (wait_queue*)malloc(sizeof(wait_queue));
    mutex->waitQueue->front = NULL;
    mutex->waitQueue->end = NULL;
    //printf("mutex created\n");

    return 0;
}

int my_pthread_mutex_lock(my_pthread_mutex_t *mutex){

	if (mutex->init != 1){
		return EINVAL;
	}

    while(__sync_lock_test_and_set(&mutex->lock, 1) == LOCKED){
        spin_lock(mutex->queueLock);
        if (mutex->lock == LOCKED){
            dequeueReady(currentNode->level);
            enqueueMutex(mutex->waitQueue, currentNode);
            spin_unlock(mutex->queueLock);
            setWaiting = 1;
            scheduler();
            //will be descheduled in the scheduler using setWaiting flag
        }
        else {
            spin_unlock(mutex->queueLock);
        }
    }
    //printf("lock acquired\n");
    //if we need to this is where we return errors and also where we handle if
    //the thread that locked the lock is exited
    return 0;
}

//unlock
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex){

	if (mutex->init != 1){
		return EINVAL;
	}

    
    spin_lock(mutex->queueLock);
    threadNode * node = dequeueMutex(mutex->waitQueue);
    mutex->lock = UNLOCKED;
    spin_unlock(mutex->queueLock);
    if (node != NULL){
        int level = node->level;
        if (level == NUM_LEVELS - 1){
            enqueueReady(level, node); 
        }
        else{
            enqueueReady(level + 1, node); 
        }
    }
    //printf("lock released\n");
    //my_pthread_yield();

    return 0;
}

int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex){

	if (mutex->lock == LOCKED){
		return EBUSY;
	}
	if (mutex->init != 1){
		return EINVAL;
	}

    threadNode *node = mutex->waitQueue->front;

    while (node != NULL){
        int level = node->level;
        if (level == NUM_LEVELS - 1){
            enqueueReady(level, node);
        }
        else {
            enqueueReady(level + 1, node);
        }
        node = node->next;
    }

    free(mutex->waitQueue);
    mutex->waitQueue = NULL;

	mutex->init = 0;
    mutex->lock = UNLOCKED;
    mutex->queueLock = UNLOCKED;
    mutex->waitQueue = NULL;
    //printf("mutex destroyed\n");
}




//////////////////////////////////////////
/*
    Helper Functions
*/
//////////////////////////////////////////

/*
saves context of current thread and determines which thread will be run
next and for how long. Also recalcuates the current thread's priority
and moves it to the correct level of ready queue. 

After a certain number of schedules, increases aging threads' priority.

Sets timer for the next thread and swaps from current context to the next
threads saved context.
*/
void scheduler(){

    
    //printf("currentThread: %d\n", currentNode->mythread->tid);

    //printf("BEFORE UPDATE\n");
    //printReady();
    updateQueues();
    //printf("AFTER UPDATE\n");
    //printReady();
    
    threadNode *topNode = NULL;
    
    int i;

    //find highest node in queue
    for (i = 0; i < NUM_LEVELS; i++){
        topNode = readyQueue->subqueues[i]->front;
        if (topNode != NULL){
            break;
        }
    }
    if (topNode == NULL){
        printf("scheduler: could not locate top thread or no more to schedule\n");
        exit(1);
    }
    //search again if topNode is currently running thread
    if (topNode->mythread->tid == currentNode->mythread->tid){
        if (topNode->next != NULL){
            topNode = topNode->next;
        }
        else {
            for (i = i + 1; i < NUM_LEVELS; i++){
                topNode = readyQueue->subqueues[i]->front;
                if (topNode != NULL){
                    //found topNode
                    break;
                }
            }
        }
        if (topNode == NULL){
            topNode = currentNode;
        }
    }
    //printf("topNode: %d\n", topNode->mythread->tid);
    //printf("topNode level: %d\n", topNode->level);
    //recalculate current threads priority
    int multiplier = readyQueue->subqueues[topNode->level]->multiplier;

    it_val.it_value.tv_sec = 50 * multiplier / 1000;
    it_val.it_value.tv_usec = (50 * multiplier * 1000) % 1000000;
    it_val.it_interval = it_val.it_value;

    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1){
        perror("error calling setitimer()");
        exit(1);
    }

    threadNode *saveCurrent = currentNode;
    currentNode = topNode;
    scheduleCount++;
    //equals 1 when a exiting thread has been freed 
    if (freeNode == 1){
        //printf("thread: %d being run with level: %d\n", topNode->mythread->tid, topNode->level);
        //printf("no thread saved\n");
        free(saveCurrent);
        freeNode = 0;
        setcontext(topNode->mythread->context);
        
    }
    else{
        //printf("swapping to thread %d from thread %d\n", topNode->mythread->tid, saveCurrent->mythread->tid);
        swapcontext(saveCurrent->mythread->context, topNode->mythread->context);
    }
    

}

/*
creates the ready queue and initializes each level
also registers signal handler(s)
*/
void initializeScheduler(){

    //printf("initializeScheduler\n");
    //create ready queue
    readyQueue = malloc(sizeof(ready_queue));
    int i;
    for (i = 0; i < NUM_LEVELS; i++){
        readyQueue->subqueues[i] = (queue_level* )malloc(sizeof(queue_level));
        readyQueue->subqueues[i]->front = NULL;
        readyQueue->subqueues[i]->end = NULL;
        readyQueue->subqueues[i]->level = i;
    }
    //set queue_level multipliers
    readyQueue->subqueues[0]->multiplier = MULT_0;
    readyQueue->subqueues[1]->multiplier = MULT_1;
    readyQueue->subqueues[2]->multiplier = MULT_2;
    readyQueue->subqueues[3]->multiplier = MULT_3;
    readyQueue->subqueues[4]->multiplier = MULT_4;
    readyQueue->subqueues[5]->multiplier = MULT_5;
    readyQueue->subqueues[6]->multiplier = MULT_6;
    readyQueue->subqueues[7]->multiplier = MULT_7;
    readyQueue->subqueues[8]->multiplier = MULT_8;
    readyQueue->subqueues[9]->multiplier = MULT_9;

    joinQueue = (join_queue*)malloc(sizeof(join_queue));
    joinQueue->front = NULL;
    joinQueue->end = NULL;

    exitQueue = (exit_queue*)malloc(sizeof(exit_queue));
    exitQueue->front = NULL;
    exitQueue->end = NULL;

    //register signal handler
    signal(SIGALRM, signal_handler);

}

//calls scheduler on SIGALRM
void signal_handler (int signum){
    scheduler();
    
}

/*
creates a threadNode to store a my_pthread_t object and adds to queue at
specified level of ready queue
*/
void enqueueReady(int level, threadNode * node){
    //printf("enqueueReady thread: %d level: %d\n", node->mythread->tid, level);
    node->next = NULL;
    queue_level * queue_level = readyQueue->subqueues[level];
    node->level = level;

    //if queue empty
    if (queue_level->front == NULL){
        queue_level->front = node;
        queue_level->end = node;
    }
    else {
        queue_level->end->next = node;
        queue_level->end = node;
    }
}

threadNode * dequeueReady(int level){
    queue_level * queue_level = readyQueue->subqueues[level];

    threadNode * node = queue_level->front;
    threadNode * returnedNode = NULL;
    if (queue_level->front == NULL){
        returnedNode = NULL;
    }
    else if (node->next == NULL){
        returnedNode = queue_level->front;
        queue_level->front = NULL;
        queue_level->end = NULL;
    }
    else {
        returnedNode = queue_level->front;
        queue_level->front = queue_level->front->next;
        returnedNode->next = NULL;
    }

    return returnedNode;

}

//takes currentNode and demotes it to another level 
void updateQueues(){
    int level = currentNode->level;
    if (joinCalled == 1){
        joinCalled = 0;         //node has been removed from readyQueue
    }
    else if (exitCalled == 1){
        exitCalled = 0;         //node has been removed from readyQueue
    }
    else if (setWaiting == 1){
        setWaiting = 0;
    }
    else if (level < NUM_LEVELS - 1){
        threadNode * node = dequeueReady(level);
        enqueueReady(level + 1, node);
    }
    else{
        threadNode * node = dequeueReady(level);
        enqueueReady(level, node);
    }
    
    if (scheduleCount % 10 == 0){
    		queue_level * top = readyQueue->subqueues[0];
    		queue_level * bottom = readyQueue->subqueues[NUM_LEVELS - 1];
		
		if (bottom->front != NULL){
			if (top->front == NULL){
				top->front = bottom->front;
				top->end = bottom->end;
				bottom->front = NULL;
				bottom->end = NULL;
			}
			else {
				top->end->next = bottom->front;
				top->end = bottom->end;
				bottom->front = NULL;
				bottom->end = NULL;
			}
			
			threadNode *node = top->front;;

		    while (node != NULL){
		    		node->level = 0;
			   	node = node->next;
		    }
			
		}
		
    }

}

void enqueueJoin(threadNode * node){
    //printf("enqueueJoin thread: %d\n", node->mythread->tid);
    node->next = NULL;
    if (joinQueue->front == NULL){
        joinQueue->front = node;
        joinQueue->end = node;
    }
    else {
        joinQueue->end->next = node;
        joinQueue->end = node;
    }
}

//calling thread is looking for a thread that called join on it
int searchJoin(int tid){
    //printf("searchJoin entered\n");
    threadNode * node = joinQueue->front;
    threadNode * prev = node;

    if (node == NULL){
        //printf("joinQueue is empty\n");
        return -1;
    }

    while (node != NULL){
        if (node->mythread->joinOn == tid){
            //printf("join complete\n");
            //if first and only node in list
            if (node->next == NULL && node == joinQueue->front){
                joinQueue->front = NULL;
                joinQueue->end = NULL;   
            }   //first node but there are nodes after
            else if (joinQueue->front == node){
                joinQueue->front = joinQueue->front->next;
            }
            else if (node->next == NULL){
                joinQueue->end = prev;
            }
            else{   //is in between nodes
                prev->next = node->next;

            }
            //prepare and enqueue node to readyQueue
            node->next = NULL;
            int level = node->level;
            if (level == NUM_LEVELS - 1){
                 enqueueReady(level, node); 
            }
            else{
                 enqueueReady(level + 1, node); 
            }
            return 0;

        }
        prev = node;
        node = node->next;
    }
    //printf("corresponding thread not found\n");
    return -1; 
}

void enqueueExit(threadNode * node){
    //printf("enqueueExit thread: %d\n", node->mythread->tid);
    node->next = NULL;
    if (exitQueue->front == NULL){
        exitQueue->front = node;
        exitQueue->end = node;
    }
    else {
        exitQueue->end->next = node;
        exitQueue->end = node;
    }
}

//calling thread is looking for a thread that it called join on
int searchExit(int tid){
    //printf("searchExit entered, looking for thread: %d\n", tid);

    threadNode * node = exitQueue->front;
    threadNode * prev = node;

    if (node == NULL){
        //printf("exitQueue is empty\n");
        return -1;
    }

    while (node != NULL){
        if (node->mythread->tid == tid){
            //printf("join complete\n");
            //if first and only node in list
            if (node->next == NULL && node == exitQueue->front){
                exitQueue->front = NULL;
                exitQueue->end = NULL;   
            }   //first node but there are nodes after
            else if (exitQueue->front == node){
                exitQueue->front = exitQueue->front->next;
            }
            else if (node->next == NULL){
                exitQueue->end = prev;
            }
            else{   //is in between nodes
                prev->next = node->next;

            }
            free(node);
            return 0;

        }
        prev = node;
        node = node->next;
    }
    //printf("corresponding thread not found\n");
    return -1; 

}

void enqueueMutex(wait_queue *waitQueue, threadNode * node){
    //printf("enqueueMutex thread: %d\n", node->mythread->tid);
    //get current thread
    node->next = NULL;
    if (waitQueue->front == NULL){
        waitQueue->front = node;
        waitQueue->end = node;
    }
    else {
        waitQueue->end->next = node;
        waitQueue->end = node;
    }
   
}

threadNode *dequeueMutex(wait_queue *waitQueue){ // dequeue front of queue
    threadNode * returnedNode = NULL;
    if (waitQueue->front == NULL){
        //printf("dequeue A\n");
        returnedNode = NULL;
    }
    else if (waitQueue->front->next == NULL){
        returnedNode = waitQueue->front;
        waitQueue->front = NULL;
        waitQueue->end = NULL;
        //printf("dequeue B\n");
    }
    else {
        returnedNode = waitQueue->front;
        waitQueue->front = waitQueue->front->next;
        returnedNode->next = NULL;
        //printf("dequeue C\n");
    }
    return returnedNode;
}

void spin_lock(volatile int lock){
    while (1){
        while (lock == LOCKED);
        if (__sync_lock_test_and_set(&lock, 1) == UNLOCKED){
            break;
        }
    }
}

void spin_unlock(volatile int lock){
    lock = UNLOCKED;
}

void printReady(){
    //printf("\nreadyQueue:\n");
    int i;
    for (i = 0; i < NUM_LEVELS; i++){
        queue_level * subqueue = readyQueue->subqueues[i];
        threadNode * node = subqueue->front;
        //printf("level %d: ", i);
        while (node != NULL){
            printf("%d ", node->mythread->tid);
            node = node->next;
        }
        printf("\n");
    }
    printf("\n");
}