#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "data.h"
#include <stdbool.h>

// memory.h - Felix Guo
// This module manages the memory model for WendyScript.

// VM Memory Limits
#define INITIAL_STACK_SIZE 1024
#define INITIAL_WORKING_STACK_SIZE 512

typedef unsigned int address;

struct stack_entry {
	char *id;
	struct data val;
	bool is_closure;
};

struct refcnt_container {
	size_t count;
	size_t refs;

	// detects a reference cycle.
	bool touched;
	struct refcnt_container* prev;
	struct refcnt_container* next;
};

struct vm;
struct memory {
	struct stack_entry* call_stack;
	struct data* working_stack;
	address working_stack_pointer;

	address frame_pointer;
	address call_stack_pointer;

	size_t call_stack_size;
	size_t working_stack_size;

	struct refcnt_container* all_containers_start;
	struct refcnt_container* all_containers_end;

	// Pointer to the end of the main() stack frame
	address main_end_pointer;

	struct vm * vm;
};

struct memory * memory_init(void);
void memory_destroy(struct memory *);

// push(t) pushes a data t into the working stack
void push_arg(struct memory *, struct data t);

// pop(line) returns the top of the working stack
struct data pop_arg(struct memory *, int line);

// ensure_working_stack_size(n) ensures it can fit another items
void ensure_working_stack_size(struct memory *, size_t additional);

// refcnt_malloc() allocates a block of memory with reference count
//   returned from MKPTR mostly, the count is 1 by default
// We use a macro here so leaks can be traced back to the caller
#define refcnt_malloc(memory, count) \
	refcnt_malloc_impl(memory, safe_calloc( \
		(count) * sizeof(struct data) + sizeof(struct refcnt_container), 1), count)
struct data *refcnt_malloc_impl(struct memory *, struct data* allocated, size_t count);

// refcnt_free() reduces the refcount by 1, and frees the heap memory
//   if the refcount is 0
void refcnt_free(struct memory * memory, struct data *ptr);

// refcnt_copy() makes a copy of the pointer, increasing refcount by 1
struct data *refcnt_copy(struct data *ptr);

// wendy_list_malloc is a helper for refcnt allocating space for a wendy
//   list but also inserting a header
#define wendy_list_malloc(memory, size) wendy_list_malloc_impl(refcnt_malloc(memory, (size) + 1), (size))
struct data* wendy_list_malloc_impl(struct data* allocated, size_t size);

// push_frame(name) creates a new stack frame (when starting a function call)
void push_frame(struct memory * memory, char* name, address ret, int line);

// push_auto_frame() creates an automatical local variable frame
void push_auto_frame(struct memory * memory, address ret, char* type, int line);

// pop_frame(is_ret) ends a function call, pops the latest stack frame
//   (including automatically created local frames IF is_ret!
//   is_ret is true if we RET instead of ending bracket
//
//   pop_frame also returns true if the popped frame is a function frame
bool pop_frame(struct memory * memory, bool is_ret, address* ret);

// push_stack_entry(id) declares a new variable in the stack frame
//   this leaves the val as EMPTY, and not NONE
struct stack_entry* push_stack_entry(struct memory * memory, char* id, int line);

// id_exist(id, search_main) returns true if id exists in the current stackframe
bool id_exist(struct memory * memory, char* id, bool search_main);

// get_address_of_id(id, line) returns address of the id given
//   requires: id exist in the stackframe
struct data* get_address_of_id(struct memory * memory, char* id, int line);

// get_value_of_id(id, line) returns the value of the id given
//   requires: id exist in the stackframe
struct data* get_value_of_id(struct memory * memory, char* id, int line);

// print_call_stack prints out the callstack
void print_call_stack(struct memory * memory, FILE* file, int maxlines);

// get_address_pos_of_id(id, line) gets the stack address of the id
address get_stack_pos_of_id(struct memory * memory, char* id, int line);

// top_arg(line) returns the pointer to top data without popping!!
struct data* top_arg(struct memory * memory, int line);

// clear_working_stack() clears the operational stack
void clear_working_stack(struct memory * memory);

// create_closure() creates a closure with the current stack frame and returns
//   the index of the closure frame
struct data *create_closure(struct memory * memory);

// unwind_stack() pops all stack frames other than the main
//   * used after each run in REPL in case REPL leaves the stack in a non-
//   stable state
void unwind_stack(struct memory * memory);

// copy_globals() copies all the variables in main from one memory stackframe
//   to another; used when creating bus threads
void copy_globals(struct memory * dest, struct memory * src);
#endif
