#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "memory.h"
#include <stdarg.h>

static char* source; 

void init_error(char* src) {
	source = src;
}

void free_error() {
	if (source) {
		free(source);	
	}
}

// error handling functions
void error(int line, char* message, ...) {
	va_list a_list;
	va_start(a_list, message);
	char* additional = va_arg(a_list, char*);
	if (additional) {
		printf(RED "Runtime Error" RESET " at line " YEL "%d" RESET
				": %s: %s\n", line, additional, message);
	}
	else {
		printf(RED "Runtime Error" RESET " at line " YEL "%d" RESET
				": %s\n", line, message);
	}
	printf("==========================\n%5s %s\n", "Line", "Source");
	// find the line from the file
	int curr_line = 0;
	size_t charindex = 0;
	size_t line_start = 0;
	while (source[charindex]) {
		if (source[charindex] == '\n') {
			curr_line++;
		
			if (curr_line >= line - 1 && curr_line <= line + 1) {	
				char line_to_print[charindex - line_start + 1];
				memcpy(line_to_print, &source[line_start], charindex - line_start);
				line_to_print[charindex - line_start] = 0;
				if (curr_line == line) {
					printf("%5d " RED "%s\n" RESET, curr_line, line_to_print);
				}
				else {
					printf("%5d %s\n", curr_line, line_to_print);
				}
				if (curr_line == line + 1) {
					break;
				}
			}				
			line_start = charindex + 1;
		}
		charindex++;
	}
	printf("==========================\n");
	print_call_stack();
	exit(1);
}
