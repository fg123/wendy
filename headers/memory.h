#ifndef MEMORY_H
#define MEMORY_H

#include "token.h"
#include "global.h"
#include <stdbool.h>

// memory.h - Felix Guo
// This module manages the memory model for WendyScript.

typedef unsigned int address;

typedef struct {
    char id[MAX_IDENTIFIER_LEN + 1];
    address val;
    bool is_closure;
} stack_entry;

typedef struct mem_block mem_block;
struct mem_block {
    unsigned int size;
    address start;
    mem_block* next;
};

token* memory;
mem_block* free_memory;
stack_entry* call_stack;
address* mem_reg_stack;

// Includes a list of closures, used to store temporary stack frames.
//   The size of the closure lists is also stored for easy iteration.
stack_entry** closure_list;
size_t* closure_list_sizes;

extern address frame_pointer;
extern address stack_pointer;
extern address closure_list_pointer;
extern address arg_pointer;

// init_memory() allocates the memory
void init_memory();

// c_free_memory() deallocates the memory
void c_free_memory();

// check_memory(line) ensures all the pointers are within the memory space
void check_memory(int line);

// garbage_collect() collects unused memory and stores it in the linked list
//   free_memory. Returns true if some memory was collected and false otherwise.
bool garbage_collect(int size);

// print_free_memory() prints out a list of the free memory blocks available
//   in Wendy
void print_free_memory();

// has_memory(size) returns true if a memory block of that size can be found
//   and false otherwise.
bool has_memory(int size);

// pls_give_memory(size) requests memory from Wendy and returns the address
//   of the requested block.
address pls_give_memory(int size, int line);

// here_u_go(a, size) returns memory to Wendy
void here_u_go(address a, int size);

// memory_pointer stores the address of the next available memory space
extern address memory_pointer;

// push_frame(name) creates a new stack frame (when starting a function call)
void push_frame(char* name, address ret, int line);

// push_auto_frame() creates an automatical local variable frame
void push_auto_frame(address ret, char* type, int line);

// pop_frame(is_ret) ends a function call, pops the latest stack frame 
//   (including automatically created local frames IF is_ret!
//   is_ret is true if we RET instead of ending bracket
// 
//   pop_frame also returns true if the popped frame is a function frame
bool pop_frame(bool is_ret, address* ret);

// push_memory(t) adds the given number of token t into the memory in order
//   and returns the address of the first one
address push_memory(token t);

// push_memory_s(t, size) finds a continuous block of size in memory and sets
//   it all to the given token t.
address push_memory_s(token t, int size);

// push_memory_a(t, size) finds a continuous block of size in memory and sets
//   it to the array a appending a list header token..
address push_memory_a(token* a, int size);

// push_memory_array(a, size) finds a continuous block of size in memory and
//   sets it to the array "a" directly.
address push_memory_array(token* a, int size);

// pop_memory() removes a token from the memory after a push operation
token pop_memory();

// replace_memory(t, a) places the token t in the address a
void replace_memory(token t, address a);

// push_stack_entry(id, t) adds a new entry into the 
// stack frame (eg variable declaration).
void push_stack_entry(char* id, address val, int line);

// copy_stack_entry(se) copies the given stack entry into the top of the call
//   stack, USE FOR CLOSURES
void copy_stack_entry(stack_entry se, int line);

// id_exist(id, search_main) returns true if id exists in the current stackframe 
bool id_exist(char* id, bool search_main);

// get_address_of_id(id, line) returns address of the id given
//   requires: id exist in the stackframe
address get_address_of_id(char* id, int line);

// get_value_of_id(id, line) returns the value of the id given
//   requires: id exist in the stackframe
token* get_value_of_id(char* id, int line);

// get_value_of_address(address, line) returns the value of the address given
token* get_value_of_address(address a, int line);

// print_call_stack prints out the callstack
void print_call_stack(int maxlines);

// get_address_pos_of_id(id, line) gets the stack address of the id
address get_stack_pos_of_id(char* id, int line);

// push_arg(t) pushes a token t into the other end of memory
void push_arg(token t, int line);

// pop_arg(line) returns the top token t at the other end of memory
token pop_arg(int line);

// top_arg(line) returns the pointer to top token without popping!!
token* top_arg(int line);

// clear_arg_stack() clears the operational stack
void clear_arg_stack();

// create_closure() creates a closure with the current stack frame and returns
//   the index of the closure frame
address create_closure();

// write_state(fp) writes the current state for debugging to the file fp
void write_state(FILE* fp);

// push_mem_reg(memory_register) saves the memory register to the stack
void push_mem_reg(address memory_register);

// pop_mem_reg() pops the saved memory register 
address pop_mem_reg();

#endif
