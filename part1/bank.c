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

account *accounts;

// NOTE: the following initializations are to pre-emptively create the option to change
// the number of accounts or number of attributes associated with accounts in the future.
// in this way, the foundation of the system remains more receptive to future updates and/or scaling.

// -do not touch-
// num_accts is updated in main() when we read the 1st line of the input file
int num_accts = 0;

// CHANGE num_attr HERE 
// if accounts in the system have more (or less) than exactly 5 attributes
int num_attr = 5;

// change if the length of the lines in the input file exceeds 100 chars
size_t len = 100;


/*  used in debugging while developing code 
int correct_pw_count = 0;
int incorrect_pw_count = 0;

int num_balance_checks = 0;
int num_withdrawals = 0;
int num_deposits = 0;
int num_transfers = 0;
*/

/****************************************************************************
auxiliary functions to make the code in main() a bit easier to read
****************************************************************************/
int check_pw(account acct, char *pw) {
    if (strcmp(acct.password, pw) == 0) {
        //correct_pw_count++;
        return 1;
    }
    else {
        //incorrect_pw_count++;
        return 0;
    }
}
void check_bal(account *acct) {
    //num_balance_checks++;
}
double amount = 0.0;
void withdraw(account *acct, char *amt) {
    amount = atof(amt);
    acct->balance -= amount;
    acct->transaction_tracker += amount;
    //num_withdrawals++;
}
void deposit(account *acct, char *amt) {
    amount = atof(amt);
    acct->balance += amount;
    acct->transaction_tracker += amount;
    //num_deposits++;
}
void transfer(account *src_acct, account *dest_acct, char *amt) {
    amount = atof(amt);
    src_acct->balance -= amount;
    src_acct->transaction_tracker += amount;
    dest_acct->balance += amount;
    //num_transfers++;
}
//****************************************************************************
//****************************************************************************

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

    char *line_buf = malloc(len);
    // determine how many accounts are in the system (number on line 1 = #accts in the system)
    getline(&line_buf, &len, fp);
    // https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
    line_buf[strcspn(line_buf, "\r\n")] = 0;
    num_accts = atoi(line_buf);

    // array of accounts
    accounts = malloc(num_accts * sizeof (account));

    double blnce = 0.0;
    double rward = 0.0;

    // for each account
    for (int i=0; i < num_accts; i++) {
        // for each attribute, init the system
        for (int j=0; j < num_attr; j++) {
            getline(&line_buf, &len, fp);
            line_buf[strcspn(line_buf, "\r\n")] = 0;
            if (j == 0) {        // index (already handling, so continue)
                continue;
            }
            else if (j == 1) {   // account number
                strcpy(accounts[i].account_number, line_buf);
            }
            else if (j == 2) {   // password
                strcpy(accounts[i].password, line_buf);
            }
            else if (j == 3) {   // initial balance
                blnce = atof(line_buf);    // convert the string to double
                accounts[i].balance = blnce;
            }
            else if (j == 4) {   // reward rate
                rward = atof(line_buf);    // convert the string to double
                accounts[i].reward_rate = rward;
            }
        }
        accounts[i].transaction_tracker = 0.0;
    }

    // create the output directory and files if they don't exist, and overwrite them if they do
    makeDir("output");
    FILE *file_p;
    char *output_filepath = malloc(100 * sizeof(char));

    for (int i=0; i < num_accts; i++) {
        sprintf(output_filepath, "./output/act_%d", i);

        file_p = fopen(output_filepath, "w");
        fprintf(file_p, "account %d:\n", i);
        fclose(file_p);
    }


    command_line token_buffer;


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
    
    // for each transaction (each line in the input file after the lines for initializing the accounts)
    while(getline(&line_buf, &len, fp) != -1) {
        line_buf[strcspn(line_buf, "\r\n")] = 0;
        token_buffer = str_filler(line_buf, " ");
        
        // for each account
        for (int k=0; k < num_accts; k++) {
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
                        for (int m=0; m < num_accts; m++) {
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

    

    // for each account, apply reward_rate * transaction_tracker
    for (int p=0; p < num_accts; p++) {
        double total_reward = accounts[p].reward_rate * accounts[p].transaction_tracker;
        accounts[p].balance += total_reward;

        // construct relative path to output file for each account
        sprintf(output_filepath, "./output/act_%d", p);

        file_p = fopen(output_filepath, "a");
        // append a snapshot of the system updating the balance to the output file
        // since the worker threads are processing transactions in parallel, the lines of Current Balance will change each time you execute the code
        // but the last line will always end up the same because it's always processing the same transactions, just in differing order.
        fprintf(file_p, "Current Balance:\t%.2f\n", accounts[p].balance);
        fclose(file_p);
    }

    // redirect terminal output from stdout to output.txt
    freopen("output.txt", "w+", stdout);

    // for each account, print the account balance in output.txt
    for (int q=0; q < num_accts; q++) {
        printf("%d balance:	%.2f\n\n", q, accounts[q].balance);
    }


    free(line_buf);
    free(output_filepath);
    free(accounts);
    fclose(fp);

    /*
    printf("\ncorrect_pw_count: %d\n", correct_pw_count);
    printf("incorrect_pw_count: %d\n", incorrect_pw_count);
    printf("num_balance_checks: %d\n", num_balance_checks);
    printf("num_withdrawals: %d\n", num_withdrawals);
    printf("num_deposits: %d\n", num_deposits);
    printf("num_transfers: %d\n", num_transfers);
    */
}