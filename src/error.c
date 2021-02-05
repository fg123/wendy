#define _GNU_SOURCE
#include "error.h"
#include "memory.h"
#include "source.h"
#include "vm.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static bool error_flag = false;

void reset_error_flag() {
	error_flag = false;
}

bool get_error_flag() {
	return error_flag;
}

char* error_message(const char* message, va_list args) {
	char* result;
	vasprintf(&result, message, args);
	return result;
}

void error_general(char* message, ...) {
	error_flag = true;
	va_list args;
	va_start(args, message);

	char* msg = error_message(message, args);
	fprintf(stderr, RED "Fatal Error: " RESET "%s\n", msg);

	// Cannot be safe, because vasprintf uses malloc!
	free(msg);
	if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
		safe_exit(1);
	}
}

void error_lexer(int line, int col, char* message, ...) {
	error_flag = true;
	va_list args;
	va_start(args, message);

	char* msg = error_message(message, args);
	fprintf(stderr, RED "Parser Error" RESET " on line " YEL "%d" RESET ": %s\n",
		line, msg);

	if (has_source()) {
		fprintf(stderr, "==========================\n%5s %s (%s)\n", "Line", "Source",
			get_source_name());
		fprintf(stderr, "%5d " RED "%s\n" RESET, line, get_source_line(line));
		fprintf(stderr, "      %*c^\n", col, ' ');
	}
	free(msg);
	if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
		safe_exit(1);
	}
}

void error_compile(int line, int col, char* message, ...) {
	error_flag = true;
	va_list args;
	va_start(args, message);

	char* msg = error_message(message, args);
	fprintf(stderr, RED "Compile Error" RESET " on line " YEL "%d" RESET ": %s\n",
		line, msg);

	if (has_source()) {
		fprintf(stderr, "==========================\n%5s %s (%s)\n", "Line", "Source",
			get_source_name());
		fprintf(stderr, "%5d " RED "%s\n" RESET, line, get_source_line(line));
		fprintf(stderr, "      %*c^\n", col, ' ');
	}
	free(msg);
	if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
		safe_exit(1);
	}
}

void assert_impl(bool condition, const char* condition_str, const char* filename, size_t line_number, const char* message, ...) {
	if (!condition) {
		error_flag = true;
		va_list args;
		va_start(args, message);
		char* msg = error_message(message, args);
		fprintf(stderr, RED "Fatal Error: " RESET "assertion (%s) failed on %s:%ld: " RESET "%s\n", condition_str, filename, line_number, msg);
		// Cannot be safe, because vasprintf uses malloc!
		free(msg);
		safe_exit(1);
	}
}

void error_runtime(struct memory* memory, int line, char* message, ...) {
	error_flag = true;
	va_list args;
	va_start(args, message);

	char* msg = error_message(message, args);
	fprintf(stderr, RED "Runtime Error" RESET " on line " YEL "%d:" RESET " %s\n", line, msg);

	if (has_source()) {
		if (!is_source_accurate()) {
			fprintf(stderr, YEL "Note: " RESET "Source was automatically loaded "
			"and may not reflect the actual source of the compiled code.\n");
		}
		fprintf(stderr, DIVIDER "\n%5s %s (%s)\n", "Line", "Source",
			get_source_name());
		int start_line = (line - 2 > 0) ? (line - 2) : 1;
		int end_line = start_line + 5;
		for (int i = start_line; i < end_line; i++) {
			if (is_valid_line_num(i)) {
				if (i == line) {
					fprintf(stderr, "%5d " RED "%s\n" RESET, i, get_source_line(i));
				}
				else {
					fprintf(stderr, "%5d %s\n" RESET, i, get_source_line(i));
				}
			}
		}
		fprintf(stderr, DIVIDER "\n");
	}
	free(msg);
	// REPL Don't print call stack unless verbose is on!
	if (!get_settings_flag(SETTINGS_REPL) || get_settings_flag(SETTINGS_VERBOSE)) {
		print_call_stack(memory, stderr, -1);
		fprintf(stderr, RED "VERBOSE ERROR DUMP\n" RESET);
		fprintf(stderr, GRN "Limits\n" RESET);
		fprintf(stderr, "Call Stack Size %zu\n", memory->call_stack_size);
		fprintf(stderr, "Working Stack Size %zu\n", memory->working_stack_size);
		fprintf(stderr, GRN "Memory\n" RESET);
		fprintf(stderr, "SP: %d 0x%X\n", memory->call_stack_pointer, memory->call_stack_pointer);
		fprintf(stderr, "AP: %d 0x%X\n", memory->working_stack_pointer, memory->working_stack_pointer);
	}
	fflush(stdout);
	if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
		safe_exit(1);
	}
}
