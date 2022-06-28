/**************************************************************
* Author: JT Kashuba                                          *
* Date: December 3rd 2021                                     *
*                                                             *
* Notes:                                                      *
*                                                             *
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "account.h"
#include "string_parser.h"

// change NUM_ATTR here if accounts in the system have more (or less) than exactly 5 attributes
#define NUM_ATTR 5

// change NUM_WORKER_THREADS here if you want to split the transactions among more (or less) than 10 threads
#define NUM_WORKER_THREADS 10

// change if the length of the lines in the input file exceeds 100 chars. temp_len is arbitrarily large
size_t temp_len = 100;  // to ensure all lines are counted and the length of the longest line is known
size_t len = 0;         // so we can dynamically allocate memory after determining len in main()


account *accounts;
char **transactions;

// uses 10 worker-threads to handle processing transactions and
// 1 banker-thread to update balances based on (reward_rate * transaction_tracker)
pthread_t tid[NUM_WORKER_THREADS+1];


// !!!!! DO NOT TOUCH!!!!---V
int NUM_ACCTS = 0;           // NUM_ACCTS is updated in main() when we read the 1st line of the input file
int NUM_TRANSACTIONS = 0;    // NUM_TRANSACTIONS is updated in main() as it reads until the end of file
int THREAD_WORKLOAD = 0;     // THREAD_WORKLOAD is updated in main() once NUM_TRANSACTIONS is found
int REMAINDER = 0;
int counter = 0;             // counter used to signal banker thread every 5k transactions (excluding check balance and invalid request aka incorrect password)
int waiting_thread_count = 0;// to enable calling the update_balance function
int total_processed = 0;
// !!!!! DO NOT TOUCH!!!!---^

pthread_mutex_t mutex;
pthread_barrier_t sync_barrier;         // barrier to make all threads start simultaneously
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;       // lock for the conditional wake up
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;   // lock for the conditional wake up

pthread_mutex_t wake_bank_mtx = PTHREAD_MUTEX_INITIALIZER; // lock for the other conditional wake up
pthread_cond_t sig_wake_bank = PTHREAD_COND_INITIALIZER;   // lock for the other conditional wake up

int alive_threads = NUM_WORKER_THREADS;
pthread_mutex_t alive = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    char** list_of_transactions;
    int flag;

}transactions_struct;


/****************************************************************************
auxiliary functions to make the code in process_transaction() a bit easier to read

part2 implementation of synchronization without race conditions, inspiration from:
https://www.geeksforgeeks.org/mutex-lock-for-linux-thread-synchronization/

****************************************************************************/
int check_pw(account acct, char *pw) {
    if (strcmp(acct.password, pw) == 0) {
        return 1;
    }
    else {
        return 0;
    }
}
void check_bal(account *acct) {
    // no functionality needed for project scope
}
double amount = 0.0;
void withdraw(account *acct, char *amt) {
    //pthread_mutex_lock(&(acct->ac_lock));
    pthread_mutex_lock(&mutex);
    amount = atof(amt);
    acct->balance -= amount;
    acct->transaction_tracker += amount;
    counter++;
    //pthread_mutex_unlock(&(acct->ac_lock));
    pthread_mutex_unlock(&mutex);
}
void deposit(account *acct, char *amt) {
    //pthread_mutex_lock(&(acct->ac_lock));
    pthread_mutex_lock(&mutex);
    amount = atof(amt);
    acct->balance += amount;
    acct->transaction_tracker += amount;
    counter++;
    //pthread_mutex_unlock(&(acct->ac_lock));
    pthread_mutex_unlock(&mutex);
}
void transfer(account *src_acct, account *dest_acct, char *amt) {
    withdraw(src_acct, amt);
    //pthread_mutex_lock(&(dest_acct->ac_lock));
    pthread_mutex_lock(&mutex);
    amount = atof(amt);
    dest_acct->balance += amount;
    //pthread_mutex_unlock(&(dest_acct->ac_lock));
    pthread_mutex_unlock(&mutex);
}

//****************************************************************************
//****************************************************************************


// (╯°□°）╯︵ ┻━┻  PROCESSING THE VALID TRANSACTIONS  ┻━┻ ︵ ヽ(°□°ヽ)
// __________________________________________________________________________

// Check balance : “C acct_num pw”
// Withdraw      : “W acct_num pw amount”
// Deposit       : “D acct_num pw amount”
// Transfer funds: “T src pw dest amount”
/****************************************************************************
 * We can see that all 4 transaction types have the account that needs it's pw
 * checked in index 1 and the pw input by the user in index 2 of our tokenized
 * array. So, we just check the password right off the bat. If the password
 * given by the user doesn't match the password associated with the account
 * given by the user, we can stop immediately and move to the next transaction. 
*****************************************************************************/
void* process_transaction (void* arg) {
    printf("Worker Thread: created but waiting\n");fflush(stdout);
    //printf("Worker Thread tid=%d: created but waiting\n", gettid());fflush(stdout);

    pthread_barrier_wait(&sync_barrier);

    printf("Worker Thread: started working\n");fflush(stdout);
    //printf("Worker Thread tid=%d: started working\n", gettid());fflush(stdout);

    command_line token_buffer;
    char **transactions_to_process = (char **) arg;
    // for each transaction this thread will process
    for (int i=0; i < THREAD_WORKLOAD; i++) {
        
        // every 5k transactions (excluding check balance + incorrect pw's), signal the banker thread and put all workers in a cond_wait state
        if (counter >= 5000) {
            pthread_mutex_lock(&mtx);
            waiting_thread_count++;
            printf("Worker Thread: Paused. | waiting_thread_count=%d | alive_threads=%d\n", waiting_thread_count, alive_threads);fflush(stdout);
            //printf("Worker Thread tid=%d: Paused. | waiting_thread_count=%d | alive_threads=%d\n", gettid(), waiting_thread_count, alive_threads);fflush(stdout);
            //sleep(1);

            // if # of waiting threads = # of live threads, broadcast to wake up banker thread
            if (waiting_thread_count == alive_threads) {
                pthread_mutex_lock(&wake_bank_mtx);
                printf("\n---------------------------------------------------------------------------------\n");
                printf("**************ALL %d alive_threads NOW WAITING, waking bank in 2 seconds**************\n", alive_threads);
                printf("---------------------------------------------------------------------------------\n\n");
                sleep(2);
                pthread_cond_broadcast(&sig_wake_bank);
                pthread_mutex_unlock(&wake_bank_mtx);
            }
            // else wait for the other worker threads to also be in the waiting state
            pthread_cond_wait(&condition, &mtx);
            printf("Worker Thread: received a wake-up signal. | counter=%d | alive_threads=%d\n", counter, alive_threads);fflush(stdout);
            //printf("Worker Thread tid=%d: received a wake-up signal. | counter=%d | alive_threads=%d\n", gettid(), counter, alive_threads);fflush(stdout);
            pthread_mutex_unlock(&mtx);
        }
        
        token_buffer = str_filler(transactions_to_process[i], " ");
        // for each account
        for (int k=0; k < NUM_ACCTS; k++) {
            // finds the account who's account_number matches the acct_num (or src) in the transaction details
            if (strcmp(accounts[k].account_number, token_buffer.command_list[1]) == 0) {
                // checks the password associated with the account against the password given during the transaction attempt
                if (check_pw(accounts[k], token_buffer.command_list[2]) == 1) {     
                    // if the password matches the one on file
                    // the transaction will take place, updating the correct account in the "accounts" array
                    // check balance
                    if (strcmp("C", token_buffer.command_list[0]) == 0) {
                        check_bal(&accounts[k]);
                    }
                    // withdrawal
                    else if (strcmp("W", token_buffer.command_list[0]) == 0) {
                        withdraw(&accounts[k], token_buffer.command_list[3]);
                    }
                    // deposit
                    else if (strcmp("D", token_buffer.command_list[0]) == 0) {
                        deposit(&accounts[k], token_buffer.command_list[3]);
                    }
                    // transfer
                    else if (strcmp("T", token_buffer.command_list[0]) == 0) {
                        // for each account (find the dest account)
                        for (int m=0; m < NUM_ACCTS; m++) {
                            // finds the account who's account_number matches the dest acct_num
                            if (strcmp(accounts[m].account_number, token_buffer.command_list[3]) == 0) {
                                transfer(&accounts[k], &accounts[m], token_buffer.command_list[4]);
                            }
                        }
                    }
                    // transaction has taken place, no need to continue looping through the remaining accounts
                    break;
                }
            }
        }

        free_command_line(&token_buffer);
        memset(&token_buffer, 0, 0);
    }
    
    printf("Worker Thread: finished processing this threads entire workload.\n");fflush(stdout);
    //printf("Worker Thread tid=%d: finished processing this threads entire workload.\n", gettid());fflush(stdout);
    // now that 'this' thread has finished processing all of it's transactions, decrement the number of live threads
    pthread_mutex_lock(&alive);
    alive_threads--;
    pthread_mutex_unlock(&alive);

    if(alive_threads == 0) {
        pthread_mutex_lock(&wake_bank_mtx);
        pthread_cond_broadcast(&sig_wake_bank);
        pthread_mutex_unlock(&wake_bank_mtx);
    }
}

void* update_balance (void* arg) {
    printf("\t_BANKER_ Thread: created but waiting\n");
    //printf("\t_BANKER_ Thread tid=%d: created but waiting\n", gettid());

	pthread_barrier_wait(&sync_barrier);
        
	printf("\t_BANKER_ Thread: started working\n");
    //printf("\t_BANKER_ Thread tid=%d: started working\n", gettid());

    FILE *fp;

    char *output_filepath = malloc(100 * sizeof(char));

    // watchdog loop, stays alert waiting for signals from process_transaction
    while(1) {
        pthread_mutex_lock(&wake_bank_mtx);
        printf("\t_BANKER_ Thread: waiting for wake-up signal\n");
        //printf("\t_BANKER_ Thread tid=%d: waiting for wake-up signal\n", gettid());
        pthread_cond_wait(&sig_wake_bank, &wake_bank_mtx);
        pthread_mutex_unlock(&wake_bank_mtx);

        pthread_mutex_lock(&mtx);
        printf("\t_BANKER_ Thread: recieved a wake-up signal. Factoring in account reward incentives in 3 seconds\n\n");fflush(stdout);
        //printf("\t_BANKER_ Thread tid=%d: recieved a wake-up signal. Factoring in account reward incentives in 3 seconds\n\n", gettid());fflush(stdout);
        sleep(3);
        // for each account, apply reward_rate * transaction_tracker
        for (int p=0; p < NUM_ACCTS; p++) {
            //pthread_mutex_lock(&(accounts[p].ac_lock));
            pthread_mutex_lock(&mutex);
            double total_reward = accounts[p].reward_rate * accounts[p].transaction_tracker;
            accounts[p].balance += total_reward;
            accounts[p].transaction_tracker = 0.0;  // reset the accounts transaction tracker since we don't want a compounding reward

            // construct relative path to output file for each account
            sprintf(output_filepath, "./output/act_%d", p);

            fp = fopen(output_filepath, "a");
            // append a snapshot of the system updating the balance to the output file
            // since the worker threads are processing transactions in parallel, the lines of Current Balance will change each time you execute the code
            // but the last line will always end up the same because it's always processing the same transactions, just in differing order.
            fprintf(fp, "Current Balance:\t%.2f\n", accounts[p].balance);
            fclose(fp);

            //pthread_mutex_unlock(&(accounts[p].ac_lock));
            pthread_mutex_unlock(&mutex);
        }
        waiting_thread_count = 0;
        total_processed += 5000;
        counter = 0;
        printf("\tTotal transactions processed=%d | alive_threads=%d \n\n", total_processed, alive_threads);
        printf("\t_BANKER_ Thread: done with another round of updates, waking up workers in 3 seconds\n\n");
        //printf("\t_BANKER_ Thread tid=%d: done with another round of updates, waking up workers in 3 seconds\n\n", gettid());
        printf("---------------------------------------------------------------------------------------------\n");
        sleep(3);
        pthread_cond_broadcast(&condition);

        pthread_mutex_unlock(&mtx);

        // if the worker threads have all finished processing their transactions, end watchdog loop
        if (alive_threads <= 0) {
            break;
        }
        sched_yield();
    }
    free(output_filepath);
}


// uses system call to exhibit behavior equivalent to the linux command "mkdir dirName"
void makeDir(char *dirName) {
    mkdir(dirName, 0777);
}


int main(int argc, char *argv[]) {   
    
    FILE *fp;
    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        perror("Error: file not found");
        return(-1);
    }

    char *temp_buf = malloc(temp_len * sizeof(char));
    // determine how many accounts are in the system (number on line 1 = #accts in the system)
    getline(&temp_buf, &temp_len, fp);
    // https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
    temp_buf[strcspn(temp_buf, "\r\n")] = 0;
    NUM_ACCTS = atoi(temp_buf);

    // array of accounts
    accounts = malloc(NUM_ACCTS * sizeof(account));

    double blnce = 0.0;
    double rward = 0.0;

    //             INITIALIZE THE ACCOUNTS IN "accounts" ARRAY
    // for each account         -       struct definition in account.h
    for (int i=0; i < NUM_ACCTS; i++) {
        // for each attribute, init the system
        for (int j=0; j < NUM_ATTR; j++) {
            getline(&temp_buf, &temp_len, fp);
            temp_buf[strcspn(temp_buf, "\r\n")] = 0;
            if (j == 0) {        // index (already handling with loop variables where needed, so continue)
                continue;
            }
            else if (j == 1) {   // account number
                strcpy(accounts[i].account_number, temp_buf);
            }
            else if (j == 2) {   // password
                strcpy(accounts[i].password, temp_buf);
            }
            else if (j == 3) {   // initial balance
                blnce = atof(temp_buf);    // convert the string to double
                accounts[i].balance = blnce;
            }
            else if (j == 4) {   // reward rate
                rward = atof(temp_buf);    // convert the string to double
                accounts[i].reward_rate = rward;
            }
        }
        accounts[i].transaction_tracker = 0.0;      // transaction_tracker used in update_balance
        // init account struct's ac_lock elements
        if (pthread_mutex_init(&(accounts[i].ac_lock), NULL) != 0) { // mutex locks
            printf("\n mutex init has failed\n");
            return 1;
        }
    }
    // init single global lock
    if (pthread_mutex_init(&mutex, NULL) != 0) {
            printf("\n mutex init has failed\n");
            return 1;
        }

    
    // create the output directory and files if they don't exist, and overwrite them if they do
    makeDir("output");
    FILE *file_p;
    char *output_filepath = malloc(100 * sizeof(char));

    for (int i=0; i < NUM_ACCTS; i++) {
        sprintf(output_filepath, "./output/act_%d", i);

        file_p = fopen(output_filepath, "w");
        fprintf(file_p, "account %d:\n", i);
        fclose(file_p);
    }

    // determine how many transactions are in the input file, as well as the length of the longest line in the file
    while(getline(&temp_buf, &temp_len, fp) != -1) {
        NUM_TRANSACTIONS++;
        if (strlen(temp_buf) > len) { len = strlen(temp_buf); }
    }
    // now that we know how many transactions there are, we can divide the work equally amongst the worker threads 
    THREAD_WORKLOAD = NUM_TRANSACTIONS / NUM_WORKER_THREADS;
    REMAINDER = NUM_TRANSACTIONS % NUM_WORKER_THREADS;
    // likewise, now that we know the length of the longest line in the file, we can dynamically allocate only as much memory as is needed
    char *line_buf = malloc(len * sizeof(char));


    // move the file pointer back to start of file
    fseek(fp, 0, SEEK_SET);
    // move the file pointer to the first transaction (1st line after the "account init" information)
    for (int i=0; i < (1 + (NUM_ACCTS * NUM_ATTR)); i++) {
        getline(&line_buf, &len, fp);
    }

    // array of pointers to transactions (still in original form, need tokenizing)
    transactions = malloc(NUM_TRANSACTIONS * sizeof(char *));

    // POPULATING THE TRANSACTIONS ARRAY
    for (int i=0; i < NUM_TRANSACTIONS; i++) {
        getline(&line_buf, &len, fp);
        line_buf[strcspn(line_buf, "\r\n")] = 0;
        transactions[i] = malloc(len * sizeof(char));
        strcpy(transactions[i], line_buf);
    }

// -------------------------------------------------------------------------------------------- //
        /*****************************************************************************
        * Now that we've finished analyzing the input file and storing it in memory, *
        * it's time to start creating threads to handle processing the transactions  *
        ******************************************************************************/
// -------------------------------------------------------------------------------------------- //

    // why is barrier count NUM_WORKERS+2? worker threads + banker thread + main thread so that a barrier_wait() call in main  
    // can simultaneously wake up all of the worker threads and bank thread once they've all been created.
    pthread_barrier_init(&sync_barrier, NULL, NUM_WORKER_THREADS+2);

    printf("===============================================================================================\n===============================================================================================\n");
    int error;
    // SPAWN THE WORKER THREADS
    for (int q=0; q < NUM_WORKER_THREADS ; q++) {
        int offset = q * THREAD_WORKLOAD;
        // if number of transactions doesn't evenly distribute amongst the worker threads
        if (REMAINDER > 0) {
            // give the remainder to the worker thread created last
            if (q == (NUM_WORKER_THREADS-1)) {
                error = pthread_create(&(tid[q]), NULL, &process_transaction, (void*) (transactions + offset));
                if (error != 0) {
                    printf("\nThread can't be created: [%s]\n", strerror(error));
                }
            }
            else {
                error = pthread_create(&(tid[q]), NULL, &process_transaction, (void*) (transactions + offset));
                if (error != 0) {
                    printf("\nThread can't be created: [%s]\n", strerror(error));
                }    
            }
        }
        else {
            error = pthread_create(&(tid[q]), NULL, &process_transaction, (void*) (transactions + offset));
            if (error != 0) {
                printf("\nThread can't be created: [%s]\n", strerror(error));
            }
        }
    }

    // SPAWN BANKER THREAD
    error = pthread_create(&(tid[NUM_WORKER_THREADS]), NULL, &update_balance, NULL);
    if (error != 0) {
        printf("\nThread can't be created: [%s]\n", strerror(error));
    }

    sleep(1);

    printf("\n\n(╯°□°）╯︵ ┻━┻       BARRIER       ┻━┻ ︵ ヽ(°□°ヽ)\n===============================================================================================\n===============================================================================================\n");
    sleep(3);
    printf("\nThe main thread will release the barrier in 5 seconds, signaling the worker threads and banker thread to start working.\n\n\n");
    //printf("\nThe main thread tid=%d will release the barrier in 5 seconds, signaling the worker threads and banker thread to start working.\n\n\n", gettid());
    sleep(5);

    pthread_barrier_wait(&sync_barrier);

    // wait for worker threads to finish processing all transactions
    for (int r=0; r < NUM_WORKER_THREADS; r++) {
        pthread_join(tid[r], NULL);
    }

    // wait for banker thread to finish updating balances
    pthread_join(tid[NUM_WORKER_THREADS], NULL);



    // redirect terminal output from stdout to output.txt
    freopen("output.txt", "w+", stdout);
    // for each account, print the account balance in output.txt
    for (int s=0; s < NUM_ACCTS; s++) {
        printf("%d balance:	%.2f\n\n", s, accounts[s].balance);
    }



    // free buffers & arrays, close file pointers, destroy locks
    free(temp_buf);
    free(line_buf);
    free(output_filepath);
    
    for (int t=0; t < NUM_ACCTS; t++) {
        pthread_mutex_destroy(&(accounts[t].ac_lock));
    }

    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&sync_barrier);
    
    free(accounts);
    fclose(fp);

    for (int u=0; u < NUM_TRANSACTIONS; u++) {
        free(transactions[u]);
    }
    free(transactions);
}