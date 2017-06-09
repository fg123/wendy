#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "memory.h"
#include <stdarg.h>
#include <stdbool.h>

static char* source; 
static bool error_flag = false;

void reset_error_flag() {
	error_flag = false;
}

bool get_error_flag() {
	return error_flag;
}

void init_error(char* src) {
	source = src;
}

void free_error() {
	if (source) {
		free(source);	
	}
}

// error handling functions
void w_error(char* message) {
	printf(RED "Wendy Error: " RESET "%s\n", message);
	fflush(stdout);
	exit(1);
}

void d_error(char* message) {
	printf(RED "Wendy Debugger Error: " RESET "%s\n", message);
	fflush(stdout);
	exit(1);
}

void error(int line, char* message, ...) {
	error_flag = true;
	va_list a_list;
	va_start(a_list, message);
	//char* additional = va_arg(a_list, char*);
	char* additional = "~";
	if (line != 0) {
		printf(RED "Runtime Error" RESET " at line " YEL "%d (0x%X)" RESET, 
			line, line);
	}
	else {
		printf(RED "Runtime Error" RESET);
	}
	if (additional && line != 0) {
		printf(": %s: %s\n", additional, message);
	}
	else {
		printf(": %s\n", message);
	}
	if (get_settings_flag(SETTINGS_STRICT_ERROR) && source) {
		printf("==========================\n%5s %s\n", "Line", "Source");
		// find the line from the file
		int curr_line = 0;
		size_t charindex = 0;
		size_t line_start = 0;
	//	printf("%s\n", source);
		while (source[charindex]) {
			if (source[charindex] == '\n' || !source[charindex + 1]) {
				curr_line++;
				if (!source[charindex + 1]) charindex++;	
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
		print_call_stack(20);
		fflush(stdout);
		exit(1);
	}
}
