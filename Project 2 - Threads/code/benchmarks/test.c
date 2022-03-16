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
int loop = 10;
  
void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;
  
    // Storing start time
    clock_t start_time = clock();
  
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds)
		;
}


void *inc_shared_counter(void *arg) {

    for(int i = 0; i < loop; i++){
        /* Implement Code Here */
        x++;
        printf("x is incremented to %d\n", x);
    }

    return NULL;
}


int main(int argc, char **argv) {

	/* Implement HERE */
	printf("Going to run two threads to increment x up to %d\n", 2 * loop);

	pthread_t tid[2];
    for (int i = 0; i < 2; i++) {
        //pthread_create(&tid[i], NULL, inc_shared_counter, NULL);
    }
    for (int i = 0; i < 2; i++){
       //pthread_join(tid[i], NULL);
    }
	delay(100);
	printf("The final value of x is %d\n", x);
	return 0;
}
