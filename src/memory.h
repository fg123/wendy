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

extern struct stack_entry* call_stack;
extern struct data* working_stack;
extern address working_stack_pointer;

extern address frame_pointer;
extern address call_stack_pointer;

extern size_t call_stack_size;
extern size_t working_stack_size;

struct refcnt_container {
	size_t count;
	size_t refs;
	struct refcnt_container* prev;
	struct refcnt_container* next;
};

extern struct refcnt_container* all_containers_start;
extern struct refcnt_container* all_containers_end;

// init_memory() initializes the memory module
void init_memory(void);

// free_memory() deallocates the memory
void free_memory(void);

// push(t) pushes a data t into the working stack
void push_arg(struct data t);

// pop(line) returns the top of the working stack
struct data pop_arg(int line);

// refcnt_malloc() allocates a block of memory with reference count
//   returned from MKPTR mostly, the count is 1 by default
// We use a macro here so leaks can be traced back to the caller
#define refcnt_malloc(count) \
	refcnt_malloc_impl(safe_calloc( \
		(count) * sizeof(struct data) + sizeof(struct refcnt_container), 1), count)
struct data *refcnt_malloc_impl(struct data* allocated, size_t count);

// refcnt_realloc() reallocates the block of memory to increase the size
#define refcnt_realloc(ptr, count) \
	refcnt_realloc_impl(safe_realloc( \
		(void*)ptr - sizeof(struct refcnt_container), \
		(count) * sizeof(struct data) + sizeof(struct refcnt_container)), count)
struct data *refcnt_realloc_impl(struct refcnt_container* realloc_ptr, size_t new_count);

// refcnt_free() reduces the refcount by 1, and frees the heap memory
//   if the refcount is 0
void refcnt_free(struct data *ptr);

// refcnt_copy() makes a copy of the pointer, increasing refcount by 1
struct data *refcnt_copy(struct data *ptr);

// wendy_list_malloc is a helper for refcnt allocating space for a wendy
//   list but also inserting a header
#define wendy_list_malloc(size) wendy_list_malloc_impl(refcnt_malloc((size) + 1), (size))
struct data* wendy_list_malloc_impl(struct data* allocated, size_t size);

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

// push_stack_entry(id) declares a new variable in the stack frame
//   this leaves the val as EMPTY, and not NONE
struct stack_entry* push_stack_entry(char* id, int line);

// id_exist(id, search_main) returns true if id exists in the current stackframe
bool id_exist(char* id, bool search_main);

// get_address_of_id(id, line) returns address of the id given
//   requires: id exist in the stackframe
struct data* get_address_of_id(char* id, int line);

// get_value_of_id(id, line) returns the value of the id given
//   requires: id exist in the stackframe
struct data* get_value_of_id(char* id, int line);

// get_value_of_address(address, line) returns the value of the address given
struct data* get_value_of_address(struct data* a, int line);

// print_call_stack prints out the callstack
void print_call_stack(FILE* file, int maxlines);

// get_address_pos_of_id(id, line) gets the stack address of the id
address get_stack_pos_of_id(char* id, int line);

// top_arg(line) returns the pointer to top data without popping!!
struct data* top_arg(int line);

// clear_working_stack() clears the operational stack
void clear_working_stack(void);

// create_closure() creates a closure with the current stack frame and returns
//   the index of the closure frame
struct data *create_closure(void);

// unwind_stack() pops all stack frames other than the main
//   * used after each run in REPL in case REPL leaves the stack in a non-
//   stable state
void unwind_stack(void);
#endif
