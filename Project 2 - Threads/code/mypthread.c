#include "mypthread.h"

Queue* threads_queue = NULL;
tcb* scheduler_thread;
int thread_count = 0;
int mutex_count = 0;
int scheduling = 1;

struct itimerval interrupt_timer = {{0, 10000}, {0, 10000}};
struct sigaction interrupt_timer_action = {0};

#define STACK_SIZE 65536

// QUEUE IMPLEMENTATION
typedef struct QueueNode {
    void* item;
    struct QueueNode* next;
    struct QueueNode* previous;
} QueueNode;

struct Queue {
    QueueNode* head;
    QueueNode* tail;
};

void enqueueHead(Queue* queue, void* item) {
    if (queue == NULL) {
        queue = malloc(sizeof(Queue));
        memset(queue, 0, sizeof(Queue));
    }

    QueueNode* new_head = malloc(sizeof(QueueNode));
    new_head->item = item;
    new_head->next = queue->head;
    new_head->previous = NULL;

    if (queue->tail == NULL)
        queue->tail = new_head;
    else if (queue->head != NULL)
        queue->head->previous = new_head;

    queue->head = new_head;
}

void enqueueTail(Queue* queue, void* item) {
    if (queue == NULL) {
        queue = malloc(sizeof(Queue));
        memset(queue, 0, sizeof(Queue));
    }

    QueueNode* new_tail = malloc(sizeof(QueueNode));
    new_tail->item = item;
    new_tail->next = NULL;
    new_tail->previous = queue->tail;

    if (queue->head == NULL)
        queue->head = new_tail;
    else if (queue->tail != NULL)
        queue->tail->next = new_tail;

    queue->tail = new_tail;
}

void dequeueHead(Queue* queue, void* item) {
    if (queue == NULL)
        return;
    if (queue->head != NULL) {
        queue->head->next->previous = NULL;
        if (queue->head->next != NULL) {
            QueueNode* next = queue->head->next;
            free(queue->head);
            queue->head = next;
        }
    }
}

void dequeueTail(Queue* queue, void* item) {
    if (queue == NULL)
        return;
    if (queue->tail != NULL) {
        queue->tail->previous->next = NULL;
        if (queue->tail->previous != NULL) {
            QueueNode* previous = queue->tail->previous;
            free(queue->tail);
            queue->tail = previous;
        }
    }
}

void dequeue(Queue* queue, void* item) {
    if (queue == NULL)
        return;
    QueueNode* current = queue->head;
    while (current != NULL) {
        if (((tcb*) current->item) == item) {
            if (current->previous != NULL)
                current->previous->next = current->next;
            if (current->next != NULL)
                current->next->previous = current->previous;
            free(current);
            return;
        }
        current = current->next;
    }
}

void moveToFront(Queue* queue, void* item) {
    dequeue(queue, item);
    enqueueHead(queue, item);
}

void moveToBack(Queue* queue, void* item) {
    dequeue(queue, item);
    enqueueTail(queue, item);
}

void emptyQueue(Queue* queue) {
    if (queue == NULL)
        return;
    QueueNode* current = queue->head;
    while (current != NULL) {
        QueueNode* next = current->next;
        free(current);
        current = next;
    }
}

tcb* findThreadFromID(Queue* queue, mypthread_t thread_id) {
    if (queue == NULL || queue->head == NULL)
        return NULL;
    QueueNode* current = queue->head;
    while (current != NULL) {
        if (((tcb*) current->item)->thread_id == thread_id)
            return current->item;
        current = current->next;
    }
    return NULL;
}

// END QUEUE STUFF

static void schedule();
void timerInterrupt();

/* create a new thread */
// pointer to thread object in caller, thread attributes, function thread is running,
int mypthread_create(mypthread_t* thread_id, pthread_attr_t* attr, void *(*function)(void*), void* arg) {
    // create Thread Control Block
    // create and initialize the context of this thread
    // allocate space of stack for this thread to run
    // after everything is all set, push this thread int
    
    // If there are no threads, initialize main and scheduler threads
    if (thread_count == 0) {
        // Create main context
        ucontext_t main_context = {0};
        getcontext(&main_context);
        main_context.uc_stack.ss_sp = malloc(STACK_SIZE);
        main_context.uc_stack.ss_size = STACK_SIZE;
        main_context.uc_link = &scheduler_thread->context;

        tcb* main_thread = malloc(sizeof(tcb));
        memset(main_thread, 0, sizeof(tcb));
        main_thread->thread_id = thread_count++;
        main_thread->state = READY;
        main_thread->context = main_context;
        main_thread->elapsed_time = 0;

        threads_queue = malloc(sizeof(Queue));
        memset(threads_queue, 0, sizeof(Queue));

        enqueueHead(threads_queue, main_thread);

        // Create scheduler context
        ucontext_t scheduler_context = {0};
        getcontext(&scheduler_context);
        
        scheduler_context.uc_stack.ss_sp = malloc(STACK_SIZE);
        scheduler_context.uc_stack.ss_size = STACK_SIZE;
        scheduler_context.uc_link = NULL;

        scheduler_thread = malloc(sizeof(tcb));
        memset(scheduler_thread, 0, sizeof(tcb));
        scheduler_thread->thread_id = thread_count++;
        scheduler_thread->state = READY;
        scheduler_thread->context = scheduler_context;
        scheduler_thread->elapsed_time = 0;

        // TODO fix timer interrupts
        interrupt_timer_action.sa_handler = timerInterrupt;
        sigaction(SIGPROF, &interrupt_timer_action, NULL);
        setitimer(ITIMER_PROF, &interrupt_timer, NULL);

        scheduling = 1;

        makecontext(&scheduler_thread->context, schedule, 0);
    }

    ucontext_t context = {0};
    getcontext(&context);
    context.uc_stack.ss_sp = malloc(STACK_SIZE);
    context.uc_stack.ss_size = STACK_SIZE;
    context.uc_link = &scheduler_thread->context;

    tcb* thread = malloc(sizeof(tcb));
    memset(thread, 0, sizeof(tcb));
    thread->thread_id = thread_count;
    *thread_id = thread_count++;
    thread->state = READY;
    thread->context = context;
    thread->elapsed_time = 0;

    enqueueHead(threads_queue, thread);

    makecontext(&thread->context, (void (*)()) function, 0, arg);

    return 0;
}

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {
    // change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context
    tcb* thread = threads_queue->head->item;
    thread->state = READY;
    moveToBack(threads_queue, thread);
    swapcontext(&thread->context, &scheduler_thread->context);

	return 0;
}

/* terminate a thread */
void mypthread_exit(void* value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
    tcb* thread = threads_queue->head->item;
    thread->state = COMPLETED;
    thread->return_values = value_ptr;

    if (thread->waiting_to_join_queue != NULL) {
        QueueNode* current = thread->waiting_to_join_queue->head;
        while (current != NULL) {
            ((tcb*) current->item)->state = READY;
            current = current->next;
        }
    }
    emptyQueue(thread->waiting_to_join_queue);
    dequeue(threads_queue, thread);
    free(thread->context.uc_stack.ss_sp);
    free(thread);
    setcontext(&scheduler_thread->context);
}

/* Wait for thread termination */
int mypthread_join(mypthread_t thread_id, void** value_ptr) {
    // wait for a specific thread to terminate
    // de-allocate any dynamic memory created by the joining thread
    tcb* thread = threads_queue->head->item;
    thread->state = WAITING;
    thread->waiting_on_thread_id = thread_id;

    tcb* waited_on_thread = findThreadFromID(threads_queue, thread_id);
    if (waited_on_thread->waiting_to_join_queue == NULL) {
        waited_on_thread->waiting_to_join_queue = malloc(sizeof(Queue));
        memset(waited_on_thread->waiting_to_join_queue, 0, sizeof(Queue));
    }
    enqueueHead(waited_on_thread->waiting_to_join_queue, thread);
	return 0;
}

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr) {
	//initialize data structures for this mutex
	mutex->mymutex_id = mutex_count++;
    mutex->value = 0;
	mutex->mutex_owner = NULL;
	mutex->access_queue = malloc(sizeof(Queue));
	memset(mutex->access_queue, 0, sizeof(Queue));

	return 0;
}

/* acquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t* mutex) {
    // use the built-in test-and-set atomic function to test the mutex
    // if the mutex is acquired successfully, enter the critical section
    // if acquiring mutex fails, push current thread into block list and //
    // context switch to the scheduler thread
    if (__sync_lock_test_and_set(&mutex->value, 1) != 0) {
        tcb* thread = threads_queue->head->item;
        enqueueHead(mutex->access_queue, thread);
        thread->state = BLOCKED;
        swapcontext(&thread->context, &scheduler_thread->context);
    }
    else {
        mutex->mutex_owner = threads_queue->head->item;
    }

    return 0;
}

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t* mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.
	if (mutex->mutex_owner == threads_queue->head->item) {
	    mutex->mutex_owner = mutex->access_queue->head->item;
	    dequeue(mutex->access_queue, mutex->access_queue->head);
	    if (mutex->mutex_owner != NULL)
	        mutex->mutex_owner->state = READY;
	}
	return 0;
}

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t* mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init
    emptyQueue(mutex->access_queue);
	return 0;
}

void timerInterrupt() {
    swapcontext(&(((tcb*) threads_queue->head->item)->context), &scheduler_thread->context);
}

/* scheduler STCF */
static void schedule() {
    while (scheduling) {
        QueueNode* current = threads_queue->head;
        tcb* current_thread = (tcb*) current->item;
        if (current_thread->state == RUNNING) {
            current_thread->state = READY;
            current_thread->elapsed_time++;
            moveToBack(threads_queue, current_thread);
            current = threads_queue->head;
        }
        tcb* shortest_thread = NULL;
        while (current != NULL) {
            current_thread = (tcb*) current->item;
            if (current_thread->state == READY) {
                if (shortest_thread == NULL || shortest_thread->elapsed_time > current_thread->elapsed_time)
                    shortest_thread = current_thread;
            }
            current = current->next;
        }
        if (shortest_thread != NULL) {
            moveToFront(threads_queue, shortest_thread);
            shortest_thread->state = RUNNING;
            swapcontext(&scheduler_thread->context, &shortest_thread->context);
        }
    }
}
