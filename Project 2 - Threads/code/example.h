#ifndef my_pthread_h
#define my_pthread_h

#include <stdio.h>
#include <ucontext.h>

//64 kB stack
#define STACK_SIZE 1024*64
//levels of ready queue
#define NUM_LEVELS 10
#define QUANTA 50/1000
//multipliers of quanta
#define MULT_0 1
#define MULT_1 1
#define MULT_2 1
#define MULT_3 2
#define MULT_4 2
#define MULT_5 2
#define MULT_6 2
#define MULT_7 3
#define MULT_8 3
#define MULT_9 3

#define LOCKED 1
#define UNLOCKED 0

//stores all user thread information
typedef struct my_pthread_t {

	ucontext_t *context;
	int tid;
	int exited;
	void *value_ptr;

	int joinOn;	//thread that calls join will set this to equal the pthread parameter
	

} my_pthread_t;

typedef struct my_pthread_attr_t {
	//placeholder
} my_pthread_attr_t;


//wraps a my_pthread_t object in a node that can be added to a linked list queue
typedef struct threadNode{
	int level;
	my_pthread_t *mythread;
	struct threadNode *next;
} threadNode;

//points to the front and end of a linked list queue at a certain level of the ready queue
//also saves the multiplier of that level
typedef struct queue_level{

	int level;
	int multiplier;
	threadNode *front;
	threadNode *end;

} queue_level;

//stores all linked list queues into an array of queues
//contains all my_pthread_t that are runnable
typedef struct ready_queue {

queue_level *subqueues[NUM_LEVELS];

} ready_queue;

typedef struct join_queue{
	threadNode *front;
	threadNode *end;
} join_queue;

typedef struct exit_queue{
	threadNode *front;
	threadNode *end;
} exit_queue;

typedef struct wait_queue{
	threadNode *front;
	threadNode *end;
} wait_queue;

typedef struct my_pthread_mutex_t{
	int init;
    volatile int lock;
    volatile int queueLock;
    wait_queue * waitQueue;
    //lockTicket* firstTicket;
} my_pthread_mutex_t;

typedef struct my_pthread_mutexattr_t {

} my_pthread_mutexattr_t;

//library functions
int my_pthread_create( my_pthread_t * thread, my_pthread_attr_t * attr, void *(*function)(void*), void * arg);
void my_pthread_yield();
void my_pthread_exit(void *value_ptr);
int my_pthread_join(my_pthread_t thread, void **value_ptr);
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const my_pthread_mutexattr_t *mutexattr);
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

//helper functions
void signal_handler(int signum);
void initializeScheduler();
void scheduler();
void enqueueReady(int level, threadNode * node);
threadNode * dequeueReady(int level);
void updateQueues();
void enqueueJoin(threadNode *node);
int searchJoin(int tid);
void enqueueExit(threadNode *node);
int searchExit(int tid);
void enqueueMutex(wait_queue *wait_queue, threadNode * node);
threadNode *dequeueMutex(wait_queue *wait_queue);
void spin_lock(volatile int lock);
void spin_unlock(volatile int lock);
void printReady();

#endif /* my_pthread_h */