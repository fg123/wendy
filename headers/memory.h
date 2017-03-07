#ifndef MEMORY_H
#define MEMORY_H

#include "token.h"
#include <stdbool.h>

// memory is a list of tokens holding values.
// call_stack/array includes all the frames and each frame has entries.
//   Each entry is either:
//		identifier -> address to previous stack 
//			identifier is either FUNC (function ret) or LOC (local automatic)
//		identifier -> address to memory
//		identifier -> function address
// frame_pointer is the address to the start to the current frame.
// stack_pointer is the address to the front of the stack, last empty

// an address is an unsigned int
// stack_entry holds char[128] -> address
// frame_pointer is an address

typedef unsigned int address;

// sizeof(stack_entry) = 128
typedef struct {
	char id[124];
	address val;
} stack_entry;

// A TOKEN IS 1032 BYTES, we allow 128mb of Memory
// 128mb * 1024kb/mb * 1024b/kb = 134217728 bytes = 129055.5 Tokens in 128 MB
// Lets choose an arbitrary closest prime because why not: 129061
//
// FIRST SLOT OF MEMORY IS RESERVED FOR NONE TOKEN
#define MEMORY_SIZE 129061
token memory[MEMORY_SIZE];

// 128bytes per entry, 8mb of stack size
// 8 * 1024 * 1024 = 8388608 bytes = 65536 entries in 8mb
#define STACK_SIZE 65536
stack_entry call_stack[STACK_SIZE];

extern address frame_pointer;
extern address stack_pointer;

// memory_pointer stores the address of the next available memory space
extern address memory_pointer;

// push_frame(name) creates a new stack frame (when starting a function call)
void push_frame(char* name);

// push_auto_frame() creates an automatical local variable frame
void push_auto_frame();

// pop_frame(is_ret) ends a function call, pops the latest stack frame 
//   (including automatically created local frames IF is_ret!
//   is_ret is true if we RET instead of ending bracket
//
//   pop_frame also returns true if the popped frame is a function frame
bool pop_frame(bool is_ret);

// push_memory(t) adds the token t into memory and returns the address of the
//   pushed token
address push_memory(token t);

// replace_memory(t, a) places the token t in the address a
void replace_memory(token t, address a);

// push_stack_entry(id, t) adds a new entry into the 
// stack frame (eg variable declaration).
void push_stack_entry(char* id, address val);

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
void print_call_stack();

// get_address_pos_of_id(id, line) gets the stack address of the id
address get_stack_pos_of_id(char* id, int line);

#endif
