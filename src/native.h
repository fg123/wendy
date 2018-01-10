#ifndef NATIVE_H
#define NATIVE_H

// native.h - Felix Guo
// Contains native implementations of some functions that can be called from
//    WendyScript (this compiler specific)
// Also keeps track of arguments!

extern char** program_arguments;
extern int program_arguments_count;

void native_call(char* function_name, int expected_args, int line);

#endif
