#include "debugger.h"
#include <stdio.h>
#include <stdbool.h>
#include <error.h>
#include <memory.h>

// Implementation of the debugger
//   Support for 128 breakpoints.
// 
// Debugger break codes: 
//   c: continue to next breakpoint
//   n: proceed to next line

#define BREAKPOOP_INT_MAX 128
char* debug_output_path = 0;
int breakpoints[BREAKPOOP_INT_MAX] = {0};
int bp_count = 0;

int breakpoint_exist(int line) {
    for (int i = 0; i < bp_count; i++) {
        if (breakpoints[i] == line) {
            return i;
        }
    }
    return -1;
}

void add_breakpoint(int line) {
//  printf("Breakpoint at: %d\n", line);
    if (breakpoint_exist(line) == -1) {
        if (bp_count == BREAKPOOP_INT_MAX) {
            error_general(OUT_OF_BREAKPOINTS);                  
        }
        else {
            breakpoints[bp_count++] = line;
        }
    }
}

void remove_breakpoint(int line) {
    int remove_point = -1;
    for (int i = 0; i < bp_count; i++) {
        if (breakpoints[i] == line) {
            remove_point = i;
            break;
        }
    }
    if (remove_point == -1) return;
    for (int i = remove_point; i < bp_count - 1; i++) {
        breakpoints[i] = breakpoints[i + 1];
    }
    bp_count--;
}

void write_wendy_state() {
    if (debug_output_path) {
        FILE* file = fopen(debug_output_path, "w");
//      printf("Writing State\n");
        fprintf(file, "WendyScript Debugger\n");
        write_state(file);  
        fclose(file);
    }
}

void debug_check(int line) {
//  printf("Debug check %d\n", line);
    int bp = breakpoint_exist(line);

    if (bp != -1) {
        write_wendy_state();
        // Wait for Input
        char buffer[3];
        while(!fgets(buffer, 3, stdin)) {};
        char c = buffer[0];
        if (c == 'c') {
            // Continue
            return;
        }
        else if (c == 'n') {
            // go to next line
            add_breakpoint(breakpoints[bp] + 1);
        }
        else {
            // fuck it 
            return;
        }
    }
}
