#ifndef DEBUGGER_H
#define DEBUGGER_H

// debugger.h - Felix Guo

extern char* debug_output_path;

// add_breakpoint(line) adds a breakpoint at the given line
void add_breakpoint(int line);

// remove_breakpoint(line) removes the breakpoint at the given line
void remove_breakpoint(int line);

// debug_check(line) given the line of the next instruction will pause if there
//   is a breakpoint and wait on STDIN for instructions for the debugger.
void debug_check(int line); 

#endif
