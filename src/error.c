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

// Error Functions:
void print_verbose_info() {
    if (get_settings_flag(SETTINGS_VERBOSE)) {
        printf(RED "VERBOSE ERROR DUMP\n" RESET);
        printf(GRN "Limits\n" RESET);
        printf("MEMORY_SIZE %d\n", MEMORY_SIZE);
        printf("STACK_SIZE %d\n", STACK_SIZE);
        printf("ARGSTACK_SIZE %d\n", ARGSTACK_SIZE);
        printf("RESERVED_MEMORY %d\n", RESERVED_MEMORY);
        printf("CLOSURES_SIZE %d\n", CLOSURES_SIZE);
        printf("MEMREGSTACK_SIZE %d\n", MEMREGSTACK_SIZE);
        printf(GRN "Memory\n" RESET);
        printf("FP: %d %x\n", frame_pointer, frame_pointer);
        printf("SP: %d %x\n", stack_pointer, stack_pointer);
        printf("AP: %d %x\n", arg_pointer, arg_pointer);
        printf("CP: %d %x\n", closure_list_pointer, closure_list_pointer);
        print_free_memory();
    }
}

char* error_message(char* message, va_list args) {
    char* result;
    vasprintf(&result, message, args);
    return result;
}

void error_general(char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    printf(RED "Fatal Error: " RESET "%s\n", msg);
    print_verbose_info();

    // Cannot be safe, because vasprintf uses malloc!
    free(msg);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}

void error_lexer(int line, int col, char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    printf(RED "Parser Error" RESET " on line " YEL "%d" RESET ": %s\n",
        line, msg);

    if (has_source()) {
        printf("==========================\n%5s %s (%s)\n", "Line", "Source",
            get_source_name());
        printf("%5d " RED "%s\n" RESET, line, get_source_line(line));
        printf("      %*c^\n", col, ' ');
    }
    print_verbose_info();
    free(msg);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}


void error_compile(int line, int col, char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    printf(RED "Compile Error (Optimize)" RESET " on line " YEL "%d" RESET ": %s\n",
        line, msg);

    if (has_source()) {
        printf("==========================\n%5s %s (%s)\n", "Line", "Source",
            get_source_name());
        printf("%5d " RED "%s\n" RESET, line, get_source_line(line));
        printf("      %*c^\n", col, ' ');
    }
    print_verbose_info();
    free(msg);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}

void error_runtime(int line, char* message, ...) {
    va_list args;
    va_start(args, message);

    char* msg = error_message(message, args);
    printf(RED "Runtime Error" RESET " on line " YEL "%d" RESET" (" YEL "0x%X"
        RESET "): %s\n", line, get_instruction_pointer(), msg);

    if (has_source()) {
        if (!is_source_accurate()) {
            printf(YEL "Note: " RESET "Source was automatically loaded "
            "and may not reflect the actual source of the compiled code.\n");
        }
        printf("==========================\n%5s %s (%s)\n", "Line", "Source",
            get_source_name());
        int start_line = (line - 2 > 0) ? (line - 2) : 1;
        int end_line = start_line + 5;
        for (int i = start_line; i < end_line; i++) {
            if (is_valid_line_num(i)) {
                if (i == line) {
                    printf("%5d " RED "%s\n" RESET, i, get_source_line(i));
                }
                else {
                    printf("%5d %s\n" RESET, i, get_source_line(i));
                }
            }
        }
        printf("==========================\n");
    }
    free(msg);
    // REPL Don't print call stack unless verbose is on!
    if (!get_settings_flag(SETTINGS_REPL) || get_settings_flag(SETTINGS_VERBOSE)) {
        print_call_stack(20);
        print_verbose_info();
    }
    fflush(stdout);
    if (get_settings_flag(SETTINGS_STRICT_ERROR)) {
        safe_exit(1);
    }
}
