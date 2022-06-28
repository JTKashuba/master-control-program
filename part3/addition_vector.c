#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

  
#define MAX_THREAD 10
#define SIZE 100000000

int global_sum = 0;
pthread_mutex_t lock; // lock for the global sum
int counter = 0; // to enable calling the periodic_print function
int waiting_thread_count = 0; // to enable calling the periodic_print function

pthread_t tid[MAX_THREAD], extra;

pthread_barrier_t sync_barrier; // barrier to make all thread start simultanously

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER; // lock for the conditional wake up
pthread_cond_t condition = PTHREAD_COND_INITIALIZER; // lock for the conditional wake up

pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER; // lock for the conditional wake up
pthread_cond_t condition1 = PTHREAD_COND_INITIALIZER; // lock for the conditional wake up

int alive_threads = MAX_THREAD;
pthread_mutex_t alive = PTHREAD_MUTEX_INITIALIZER; // alive

void* vec_add(void* arg)
{
    printf("Worker Thread : %d created but waiting\n", gettid());

	pthread_barrier_wait(&sync_barrier);
        
	printf("Worker Thread : %d started working\n", gettid());


    int* a = (int *) arg;
    //printf("%d\n ", a[0]);

    for (int i=0; i < SIZE/MAX_THREAD; i++) {
        pthread_mutex_lock(&lock);
        global_sum += a[i];
        counter++;
        pthread_mutex_unlock(&lock);
        
        if (counter >= SIZE/5) {
            //pthread_mutex_lock(&mtx1);
            //pthread_cond_broadcast(&condition1);
            //pthread_mutex_unlock(&mtx1);
        
            pthread_mutex_lock(&mtx);
            //printf("Worker Thread : %d locked\n", gettid());
            //printf("Worker Thread : %d locked thread_count %d counter %d\n", gettid(), waiting_thread_count, counter); fflush(stdout);
            waiting_thread_count++;
            // if # of waiting threads = # of live threads, broadcast to wake up banker thread
            if(waiting_thread_count == alive_threads){
                pthread_mutex_lock(&mtx1);
                pthread_cond_broadcast(&condition1);
                pthread_mutex_unlock(&mtx1);
            }
            // else wait for the other worker threads to also be in the waiting state
            pthread_cond_wait(&condition, &mtx);
            //printf("Worker Thread : %d unlocked thread_count %d counter %d\n", gettid(), waiting_thread_count, counter);fflush(stdout);
            pthread_mutex_unlock(&mtx);
        }
    }
    pthread_mutex_lock(&alive);
    alive_threads--;
    pthread_mutex_unlock(&alive);

    if(alive_threads == 0) {
        pthread_mutex_lock(&mtx1);
        pthread_cond_broadcast(&condition1);
        pthread_mutex_unlock(&mtx1);
    
    }
 
    return NULL;
}

void* periodic_print(void* arg)
{
    printf("Periodic Thread : %d created but waiting\n", gettid());

	pthread_barrier_wait(&sync_barrier);
        
	printf("Periodic Thread : %d started working\n", gettid());


    while(1){ 
        pthread_mutex_lock(&mtx1);
        pthread_cond_wait(&condition1, &mtx1);
        pthread_mutex_unlock(&mtx1);



        //if (waiting_thread_count >= MAX_THREAD) {
        //if (counter >= SIZE/10) {

        pthread_mutex_lock(&mtx);
        //printf("Current global sum %d\n", global_sum);fflush(stdout);
        printf("WoW I just woke up, now I will do my staffs because all the threads are waiting/finished\n");fflush(stdout);
        for(int j=0; j < 100000000; j++){
            // do stuff in this thread
        } 
        waiting_thread_count = 0;

        counter = 0;
        pthread_cond_broadcast(&condition);

        pthread_mutex_unlock(&mtx);
        //printf("Current global sum %d\n", global_sum);fflush(stdout);

        //}

        if(global_sum >= SIZE) {
            break;
        }
	sched_yield();
    } 

    return NULL;
}
 
int main(void)
{
    int number = 0, i;
    int error;
    int total_thread = MAX_THREAD; // one extra for the periodic thread
    int* a = (int *) malloc (sizeof(int)*SIZE);


    // initializing the lock for the global sum
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }

    pthread_barrier_init(&sync_barrier,NULL, MAX_THREAD + 2); // MAX_THREAD + 2 = worker threads + 1 periodic thread + main thread



    for(int i = 0; i < SIZE; i++){
        a[i] = 1;
    }
  
    for (i = 0 ; i < total_thread; i++){
        int b  =  i * SIZE/MAX_THREAD;
        error = pthread_create(&(tid[i]), NULL, &vec_add, (void *) (a + b));
        if (error != 0)
            printf("\nThread can't be created :[%s]", strerror(error));
    }

    error = pthread_create(&extra, NULL, &periodic_print, NULL);
    if (error != 0)
        printf("\nThread can't be created :[%s]", strerror(error));
 
    sleep(3);

    printf("The main thread %d will release all the thread shortly\n", gettid());
    pthread_barrier_wait(&sync_barrier);


    for (int i = 0 ; i < total_thread; i++){
        pthread_join(tid[i], NULL);
    }
    pthread_join(extra, NULL);


    printf("\nGlobal Sum %d\n", global_sum);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&mtx);
    pthread_barrier_destroy(&sync_barrier);

 

    return 0;
}
