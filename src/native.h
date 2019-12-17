#ifndef NATIVE_H
#define NATIVE_H

#include "vm.h"

// native.h - Felix Guo
// Contains native implementations of some functions that can be called from
//    WendyScript (this compiler specific)
// Also keeps track of arguments!

extern char** program_arguments;
extern int program_arguments_count;

void native_call(struct vm* vm, char* function_name, int expected_args);

void native_bus_init(void);
void native_bus_destroy(void);

#endif
