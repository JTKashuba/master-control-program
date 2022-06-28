#ifndef STRING_PARSER_H_
#define STRING_PARSER_H_


#define _GNU_SOURCE


typedef struct
{
    char** command_list;
    int num_token;
}command_line;

// this function returns the number of tokens needed for 
// the string array based on the delimeter
int count_token (char* buf, const char* delim);

// this function can tokenize a string to token arrays based 
// on a specified delimeter, it returns a struct variable
command_line str_filler (char* buf, const char* delim);


// this function safely frees the memory of all the tokens within the array
void free_command_line(command_line* command);


#endif /* STRING_PARSER_H_ */
