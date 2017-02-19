#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"
#include <readline/readline.h>
#include "stack.h"
#include "memory.h"


// WendyScript Interpreter in C
// By: Felix Guo
// 
// main.c: used to handle REPL and calling the interpreter on a file.

const char* WENDY_VERSION = "Wendy 1.1";
const unsigned int INPUT_BUFFER_SIZE = 2048;

int main(int argc, char** argv) {
	// ADDRESS 0 REFERS TO NONE_TOKEN
	push_memory(none_token());
	// ADDRESS 1 REFERS TO EMPTY RETURNS TOKEN
	push_memory(make_token(NONERET, make_data_str("<noneret>")));
	if (argc > 2) {
		printf("Usage: wendy or wendy [file]\n");
		return 1;
	}
	else if (argc == 1) {
		printf("Welcome to %s\nCreated by: Felix Guo\n", WENDY_VERSION);
		printf("T%zd-D%zd-SE%zd\n", sizeof(token), sizeof(data), sizeof(stack_entry));
		char *input_buffer;
		// ENTER REPL MODE
		push_frame();
		while (1) {
			input_buffer = readline("> ");
			if(!input_buffer) return 0;
			run(input_buffer);
			free(input_buffer);
		}
		// don't really have to pop
		// pop_frame(true);
	}
	else {
		// FILE READ MODE
		long length = 0;
		char* buffer;
		FILE * f = fopen(argv[1], "r");
		if (f) {
			fseek (f, 0, SEEK_END);
			length = ftell (f);
			fseek (f, 0, SEEK_SET);
			buffer = malloc(length + 1); // + 1 for null terminator
			if (buffer) {
				fread (buffer, 1, length, f);
				buffer[length] = '\0'; // fread doesn't add a null terminator!

				push_frame();
				run(buffer);
			}
			if (!last_printed_newline) {
				printf("\n");
			}
			free(buffer);
			fclose (f);
		}
		else {
			printf("Error reading file!\n");
		}
	}
}
