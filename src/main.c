#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"
#include "stack.h"
#include "memory.h"
#include "error.h"
#include "execpath.h"
#include "preprocessor.h"
#include "tests.h"
#include "macros.h"
#include "scanner.h"
#include <string.h>
#include "debugger.h"

#ifdef _WIN32

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  char* cpy = malloc(INPUT_BUFFER_SIZE);
  fgets(cpy, INPUT_BUFFER_SIZE, stdin);
  return cpy;
}

/* Otherwise include the editline headers */
#else
#include <readline/readline.h>
#endif

// WendyScript Interpreter in C
// By: Felix Guo
// 
// main.c: used to handle REPL and calling the interpreter on a file.

void invalid_usage() {
	printf("usage: wendy [file] [-p-dump file] [-nogc] [-c file] [-d file b1 b2 ...]\n");
	printf("    file         : is the WendyScript file to be preprocessed and run by the interpreter.\n");
	printf("    -p-dump file : outputs each step of the preprocessing process.\n");
	printf("    -no-gc		 : disables garbage-collection.\n");
	printf("    -c file		 : only compiles and writes a preprocessed copy to the given file.\n");
	printf("\n-d file b1 b2 ...\n	Enables debugging mode, with an output file and an initial set of breakpoints.");


	exit(1);
}

void process_options(char** options, int len) {
	for (int i = 0; i < len; i++) {
		if (strcmp("-p-dump", options[i]) == 0) {
			if (i == len - 1) {
				invalid_usage();	
			}
			p_dump_path = options[i + 1];
			i++;
		}
		else if (strcmp("-c", options[i]) == 0) {
			if (i == len - 1) {
				invalid_usage();	
			}
			compile_path = options[i + 1];
			printf("Compiling file into %s.\n", compile_path);
			i++;
		}
		else if (strcmp("-d", options[i]) == 0) {
			if (i == len - 1) {
				invalid_usage();
			}
			debug_output_path = options[i + 1];
			i++;
			for (; i < len; i++) {
				add_breakpoint(atoi(options[i]));
			}
		}
		else if (strcmp("-nogc", options[i]) == 0) {
			enable_gc = false;	
		}
		else {
			invalid_usage();
		}
	}
}

int main(int argc, char** argv) {
	init_memory();

	if (argc == 1) {
		printf("Welcome to %s\nCreated by: Felix Guo\n", WENDY_VERSION);
		run_tests();
		char* path = get_path();
		printf("Path: %s\n", path);
		free(path);
		printf("T(%zd=%zd+%zd+D(%zd))-SE%zd\n", sizeof(token), sizeof(token_type), sizeof(int), sizeof(data), sizeof(stack_entry));
		char *input_buffer;
		// ENTER REPL MODE
		push_frame("main", 0);
		while (1) {
			input_buffer = readline("> ");
			if(!input_buffer) return 0;
		//	init_error(input_buffer);
			run(input_buffer);
			free(input_buffer);
		}
		// don't really have to pop
		// pop_frame(true);
		return 0;
	}
	if (argc != 2) {
		process_options(&argv[2], argc - 2);
	}
	if (argc >= 2) {
		// FILE READ MODE
		long length = 0;
		int filelength = strlen(argv[1]);

		// Determine file type
		char header[40];
		FILE *file = fopen(argv[1], "r");
		if (!file) {
			printf("Error opening file to determine type.\n");
			exit(1);
		}
		fscanf(file, "%s", header);
		fclose(file);
		if (strcmp("WendyObj", header) == 0) {
			FILE *f = fopen(argv[1], "r");
			char* buffer;
			if (f) {
				size_t size = 0;
				token* tokens = file_to_tokens(f, &size);
				//print_token_list(tokens, size);
				push_frame("main", 0);
				run_tokens(tokens, size);
			
			}
			if (!last_printed_newline) {
				printf("\n");
			}
			fclose (f);
		}
		else {
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

					push_frame("main", 0);
			//		init_error(buffer);
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
	c_free_memory();
}
