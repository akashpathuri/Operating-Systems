#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../worker.h"
#include <time.h>

/* A scratch program template on which to call and
 * test worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */
int x = 0;
int loop = 1000;
  
void delay(int number_of_seconds){
	// Converting time into milli_seconds
	int milli_seconds = 1000000 * number_of_seconds;

	// Storing start time
	clock_t start_time = clock();

	// looping till required time is not achieved
	while (clock() < start_time + milli_seconds){
		//printf("looping ");
	};

}


void *inc_shared_counter(void *arg) {
	int y = 0;
    for(int i = 0; i < loop; i++){
        /* Implement Code Here */
        x++;
		y++;
        printf("x is incremented to %d\n", x);
    }
	//printf("x is incremented to %d\n", y);
	worker_exit(NULL);
}


int main(int argc, char **argv) {

	/* Implement HERE */
	int threads = 2;
	printf("Going to run two threads to increment x up to %d\n", threads * loop);

	worker_t tid[threads];
    for (int i = 0; i < threads; i++) {
        worker_create(&tid[i], NULL, inc_shared_counter, NULL);
    }
	delay(5);
    for (int i = 0; i < threads; i++){
       worker_join(&tid[i], NULL);
    }
	
	printf("The final value of x is %d\n", x);
	return 0;
}
