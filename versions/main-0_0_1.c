#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// If compiling on Windows compile these functions
#ifdef _WIN32 // _LP64 for linux
#include <string.h>

// Buffer for suer input of size 2048
static char buffer[2048];

// Fake readline function
char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1); // +1 reserved to \0
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0'; // Change last string character to \0
	return cpy;
}

// Fake add_history function (for no errors)
void add_history(char* unused) {}


// Otherwise include the editline header
#else
// To be able to use linux arrow keys
#include <editline.h>

#endif

// cc -std=c99 -Wall -ledit main.c -o prompt


int main(int argc, char const *argv[]) {
	puts("Vispy Version 0.0.1");
	puts("Press Ctrl+c to Exit");
	puts("Type \"author\" for more information.\n");


	while(1) {
		// fputs("Vispy> ", stdout);
		char *input = readline("Vispy> ");

		// Add input to history
		add_history(input);

		if(strcmp(input, "author")==0) {
			puts("Author: Vako");
			continue;
		}


		// Read a line of user input of maximum size 2048
		// fgets(input, 2048, stdin);
		printf("Command \"%s\" not found \n", input);

		// Attempt to parse the user input

		// Free input memory
		free(input);
	}

