#ifndef VM_H
#define VM_H

#include "data.h"
#include "memory.h"
#include "operators.h"
#include <stdint.h>

// vm.h - Felix Guo
// Executes a stream of bytecode based on instructions in [codegen] by
//   interfacing with [memory]

struct vm {
    int line;
    address instruction_ptr;
    uint8_t* bytecode;
    size_t bytecode_size;
    char* last_pushed_identifier;

    struct memory* memory;
};

struct vm *vm_init(void);
void vm_destroy(struct vm * vm);

// vm_run(bytecode) runs the given bytecode.
void vm_run(struct vm * vm, uint8_t* bytecode, size_t size);
void vm_cleanup_if_repl(struct vm * vm);

// print_current_bytecode() prints the current executing bytecode
void print_current_bytecode(struct vm * vm);
#endif
