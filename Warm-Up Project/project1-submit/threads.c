#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

//Michael Elkhouri, mre66, ilab machine: assembly.cs.rutgers.edu
//Akash Pathuri, arp229, same machine

int x = 0;
int loop = 10000;

/* Counter Incrementer function:
 * This is the function that each thread will run which
 * increments the shared counter x by LOOP times.
 *
 * Your job is to implement the incrementing of x
 * such that is sychronized among threads
 *
 */
void *inc_shared_counter(void *arg) {

    int i;

    for(i = 0; i < loop; i++){
        /* Implement Code Here */
        x++;
        printf("x is incremented to %d\n", x);
    }

    return NULL;
}


/* Main function:
 * This is the main function that will run.
 *
 * Your job is two create two threads and have them
 * run the inc_shared_counter function().
 */
int main(int argc, char *argv[]) {

    if(argc != 2){
        printf("Bad Usage: Must pass in a integer\n");
        exit(1);
    }

    loop = atoi(argv[1]);

    printf("Going to run two threads to increment x up to %d\n", 2 * loop);


    /* Implement Code Here */ 
    pthread_t tid[2];
    for (int i = 0; i < 2; i++) {
        pthread_create(&tid[i], NULL, inc_shared_counter, NULL);
    }
    for (int i = 0; i < 2; i++){
       pthread_join(tid[i], NULL);
    }

    printf("The final value of x is %d\n", x);

    return 0;
}