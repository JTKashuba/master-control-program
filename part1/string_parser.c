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
#include "string_parser.h"

#define _GNU_SOURCE

int count_token (char* buf, const char* delim)
{
	/* Much of the code in this function comes nearly directly from
	 * the strtok(3) Linux manual page
	 * https://man7.org/linux/man-pages/man3/strtok.3.html 
	 */

	char *str1, *token;
	char *saveptr1;
	int count = 1;
	int str_len;

	//printf(" count_token start buf: '%s'\n", buf);
	// strip newline character
	strtok_r(buf, "\n", &saveptr1);
	//printf(" count_token buf after strtok: '%s'\n", buf);
	str1 = malloc(sizeof(char)*strlen(buf) + 1);
	strcpy(str1, buf);

	//printf(" count_token strcpy buf: '%s'\n", str1);

	// check for NULL string
	if (str1[0] == '\0') {
		exit(EXIT_FAILURE);
	}

	if (str1[0] == delim[0]) {
		count--;
	}

	str_len = strlen(str1);
	//printf("str_len: %d\n", str_len);
	if (str1[str_len - 1] == delim[0]) {
		count--;
	}

	for (int i = 0; i < str_len; i++) {
		if (buf[i] == delim[0]) {
			//printf("count in loop: %d\n", count);
			count++;
		}

	}
	
	free(str1);
	//printf("count: %d\n", count);
	return count;
}

command_line str_filler (char* buf, const char* delim)
{
	char *token, *str1, *saveptr1;

	// strip newline character
	//strtok_r(buf, "\n", &saveptr1);
	str1 = malloc(sizeof(char)*strlen(buf) + 1);
	strcpy(str1, buf);
	// create command_line variable to be filled and returned
	command_line ret_val;

	//printf(" str_filler start buf: '%s'\n", buf);
	//printf(" str_filler delim:     '%s'\n", delim);

	// count the number of tokens with count_token function, set num_token
	ret_val.num_token = count_token(buf, delim);
	//printf("ret_val.num_token = %d\n", ret_val.num_token);

	// malloc memory for command_list inside command_line variable 
	// based on the number of tokens (+1 for NULL return from last call of strtok_r)
	ret_val.command_list = (char**) malloc(sizeof(char*) * (ret_val.num_token + 1));
	if (ret_val.command_list == NULL) {
		printf("exit_failure in 1st malloc (char**)");
		exit(EXIT_FAILURE);
	}

	token = strtok_r(buf, delim, &saveptr1);
	ret_val.command_list[0] = (char*) malloc(sizeof(char) * (strlen(token) + 1));
	if (ret_val.command_list[0] == NULL) {
		printf("exit_failure in 2nd malloc (char*) for index [0]");
		exit(EXIT_FAILURE);
	}
	//printf("0th token (before loop): %s\n", token);
	strcpy(ret_val.command_list[0], token);


	//printf("ENTERING FOR LOOP\n");
	for (int i = 1; i < ret_val.num_token; i++) {
		// use function strtok_r to find out the tokens
		token = strtok_r(NULL, delim, &saveptr1);
		//printf("%d'th token (in for loop): %s\n", i, token);

		// malloc each index of the array with the length of tokens
		ret_val.command_list[i] = (char*) malloc(sizeof(char) * (strlen(token)+1) );
		if (ret_val.command_list[i] == NULL) {
			printf("exit_failure in 3rd malloc inside loop for all other [i]");
			exit(EXIT_FAILURE);
		}

		// fill command_list array with tokens, and fill last spot with NULL.
		strcpy(ret_val.command_list[i], token);
	}

	// insert NULL into the last index
	ret_val.command_list[ret_val.num_token] = NULL;
	free(str1);
	return ret_val;
}


void free_command_line(command_line* command)
{
	for (int i = 0; i <= command->num_token; i++) {
		free(command->command_list[i]);
	}
	free(command->command_list);
}
