#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../worker.h"
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <ucontext.h>
/* A scratch program template on which to call and
 * test worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */
int x = 0;
int loop = 10;
  
void delay(int number_of_seconds){
	// Converting time into milli_seconds
	int milli_seconds = 1000 * number_of_seconds;

	// Storing start time
	clock_t start_time = clock();

	// looping till required time is not achieved
	while (clock() < start_time + milli_seconds){
		//printf("looping ");
	};

}


void *inc_shared_counter(void *arg) {
    for(int i = 0; i < loop; i++){
        /* Implement Code Here */
        x++;
        printf("x is incremented to %d\n", x);
    }
	exit(1);
}


int main(int argc, char **argv) {

	/* Implement HERE */
	printf("Going to run two threads to increment x up to %d\n", 2 * loop);

	// ucontext_t this, that;
	// if (getcontext(&new_context) == -1){
	// 	perror("getcontext: could not get parent context\n");
	// 	exit(1);
	// }
	// that.uc_link = 0;
	// that.uc_stack.ss_sp = malloc(STACK_SIZE);
	// that.uc_stack.ss_size = STACK_SIZE;
	// that.uc_stack.ss_flags = 0;
	// if(that.uc_stack.ss_sp == 0){
	// 	perror("malloc: could not allocate stack");
	// 	return -1;
	// }//remove if not used
	// printf("make: \n");
	// makecontext(&that,(void *)&inc_shared_counter, 1, arg);
	// printf("set: \n");
	// setcontext(&that);
	// printf("After set: \n");




	int threads = 1;
	worker_t tid[threads];
    for (int i = 0; i < threads; i++) {
        worker_create(&tid[i], NULL, inc_shared_counter, NULL);
		printf("Worker %d created\n", i);
    }
	printf("Before delay");
	delay(5);

    for (int i = 0; i < threads; i++){
        worker_join(tid[i], NULL);
		printf("Worker %d joined.\n", i);
    }
	
	printf("The final value of x is %d\n", x);
	return 0;
}
