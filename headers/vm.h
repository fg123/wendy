#ifndef VM_H
#define VM_H

#include "data.h"
#include "memory.h"
#include "operators.h"
#include <stdint.h>

// vm.h - Felix Guo
// Executes a stream of bytecode based on instructions in [codegen] by
//   interfacing with [memory]

// vm_run(bytecode) runs the given bytecode.
void vm_run(uint8_t* bytecode, size_t size);
void vm_cleanup_if_repl();

// get_instruction_pointer() returns the current instruction pointer.
address get_instruction_pointer();

// print_current_bytecode() prints the current executing bytecode
void print_current_bytecode();
#endif
