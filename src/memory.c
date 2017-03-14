#include "memory.h"
#include <string.h>
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

// Memory.c, provides functions for the interpreter to manipulate the local 
//   WendyScript memory
#define MEMORY_SIZE 129061
#define STACK_SIZE 100000


address frame_pointer = 0;
address stack_pointer = 0;
address memory_pointer = 0;
address arg_pointer = 0;

static const char FUNCTION_START_CHAR = '>';
// pointer to the end of the main frame
address main_end_pointer = 0; 

void init_memory() {
	memory = calloc(MEMORY_SIZE, sizeof(token));
	call_stack = calloc(STACK_SIZE, sizeof(stack_entry));
	arg_pointer = MEMORY_SIZE - 1;
}

void free_memory() {
	free(memory);
	free(call_stack);
}

void check_memory() {
	// Check stack
	if (stack_pointer >= STACK_SIZE) {
		printf("Stack at %d with limit %d!", stack_pointer, STACK_SIZE);
		error(0, STACK_OVERFLOW); 
	} 
	// Check memory, if the two ends overlap
	if (memory_pointer >= arg_pointer) {
		printf("Memory at %d with arg_ptr at %d with limit %d!", 
				memory_pointer, arg_pointer, MEMORY_SIZE);
		error(0, MEMORY_OVERFLOW);
	}

}

void push_frame(char* name, address ret) {
	// store current frame pointer
	stack_entry new_entry = { "> ", frame_pointer };
	strcat(new_entry.id, name);
	strcat(new_entry.id, "()");
	// this new entry will be stored @ location stack_pointer, so we move the 
	//   frame pointer to this location
	frame_pointer = stack_pointer;
	// add and increment stack_pointer
	call_stack[stack_pointer++] = new_entry;
	check_memory();
	stack_entry ne2 = { "RET", ret }; 
	call_stack[stack_pointer++] = ne2;
	check_memory();
	// pointer to self
	
}

void push_auto_frame(address ret) {
	// store current frame pointer
	stack_entry new_entry = { "<auto_frame>", frame_pointer };
	frame_pointer = stack_pointer;
	call_stack[stack_pointer++] = new_entry;
	
	check_memory();

	stack_entry ne2 = { "RET", ret }; 
	call_stack[stack_pointer++] = ne2;
	check_memory();
}

bool pop_frame(bool is_ret, address* ret) {
	//printf("POPPED RET:%d\n", is_ret);
	//print_call_stack();
	// trace back until we hit a FUNC 
	address trace = frame_pointer;
	if (is_ret)
	{
		// character [ is a function frame pointer, we trace until we find it
		while (call_stack[trace].id[0] != FUNCTION_START_CHAR) {
			trace = call_stack[trace].val;
		}
	}
//	printf("NEW STACK POINTER IS %d\n", trace);
	// trace is now the address in the call stack of the start of the function
	// stack_pointer is now that address
	// frame pointer is going back 1 frame to the value trace holds
	
	// return value is 1 slot under the function header
	*ret = call_stack[trace + 1].val;
//	printf("Ret:%d\n", *ret);
	stack_pointer = trace;
	frame_pointer = call_stack[trace].val;
//	printf("NEW FP IS %d\n", frame_pointer);
	// function ret or popped a function
	return (is_ret || call_stack[trace].id[0] == FUNCTION_START_CHAR);
}

void print_call_stack() {
	printf("\nDump: Stack Trace\n");
	for (int i = 0; i < stack_pointer; i++) {
		if (frame_pointer == i) {
			if (call_stack[i].id[0] == FUNCTION_START_CHAR) {
				printf("%5d FP-> [" RED "%s" RESET " -> %d: ", i, call_stack[i].id, 
						call_stack[i].val);
			}
			else {
				printf("%5d FP-> [%s -> %d: ", i, call_stack[i].id, 
						call_stack[i].val);
			}
			print_token_inline(&memory[call_stack[i].val], stdout);
			printf("]\n");
		}
		else {
			if (call_stack[i].id[0] == FUNCTION_START_CHAR) {
				printf("%5d      [" RED "%s" RESET " -> %d: ", i, call_stack[i].id, 
						call_stack[i].val);
			}
			else {
				printf("%5d      [%s -> %d: ",i, call_stack[i].id, call_stack[i].val);
			}
			print_token_inline(&memory[call_stack[i].val], stdout);
			printf("]\n");
			
		}
	}
	printf("===============\n");
}

void push_arg(token t) {
//	printf("pushed ");
//	print_token(&t);
	memory[arg_pointer--] = t;
	check_memory();
}

token pop_memory() {
	if (memory_pointer != MEMORY_SIZE - 1) {
		return memory[--memory_pointer];
	}
	printf("MEMORY IS EMPTY\n");
	return none_token();
}

token pop_arg() {
	if (arg_pointer != MEMORY_SIZE - 1) {
		return memory[++arg_pointer];
	}
	printf("ARGSTACK is EMPTY!\n");
	return none_token();
}

address push_memory(token t) {
//	printf("Memory: %d\n", memory_pointer);
	memory[memory_pointer++] = t;
	check_memory();
	return memory_pointer - 1;
}

void replace_memory(token t, address a) {
	if (a < MEMORY_SIZE) {
		memory[a] = t;
	}
	else {
		error(t.t_line, MEMORY_REF_ERROR);
	}
}

void push_stack_entry(char* id, address val) {
	stack_entry new_entry;
	new_entry.val = val;
	strncpy(new_entry.id, id, 124);	
	call_stack[stack_pointer++] = new_entry;
	if (frame_pointer == 0) {
		// currently in main function
		main_end_pointer = stack_pointer;
	}
	check_memory();
}

address get_fn_frame_ptr() {
	address trace = frame_pointer;
	while (call_stack[trace].id[0] != FUNCTION_START_CHAR) {
		trace = call_stack[trace].val;
	}
	return trace;
}

bool id_exist(char* id, bool search_main) {
	// avoid the location at frame_pointer because that's the start and stores
	for (int i = get_fn_frame_ptr() + 1; i < stack_pointer; i++) {
		if (strcmp(id, call_stack[i].id) == 0) {
			return true;
		}
	}
	if (search_main) {
		for (int i = 0; i < main_end_pointer; i++) {
			if (strcmp(id, call_stack[i].id) == 0) {
				return true;
			}
		}
	}
	return false;
}

address get_address_of_id(char* id, int line) {
	if (!id_exist(id, true)) {
		error(line, ID_NOT_FOUND, id);
		return 0;
	}
	for (int i = get_fn_frame_ptr() + 1; i < stack_pointer; i++) {
		if (strcmp(id, call_stack[i].id) == 0) {
			return call_stack[i].val;	
		}
	}
	for (int i = 0; i < main_end_pointer; i++) {
		if (strcmp(id, call_stack[i].id) == 0) {
			return call_stack[i].val;	
		}
	}
	return 0;
}

address get_stack_pos_of_id(char* id, int line) {
	if (!id_exist(id, true)) {
		
		error(line, ID_NOT_FOUND, id);
		return 0;
	}
	for (int i = get_fn_frame_ptr() + 1; i < stack_pointer; i++) {
		if (strcmp(id, call_stack[i].id) == 0) {
			return i;
		}
	}
	for (int i = 0; i < main_end_pointer; i++) {
		if (strcmp(id, call_stack[i].id) == 0) {
			return i;
		}
	}
	return 0;
}

token* get_value_of_id(char* id, int line) {
	return get_value_of_address(get_address_of_id(id, line), line);
}

token* get_value_of_address(address a, int line) {
	if (a < MEMORY_SIZE && a < memory_pointer) {
		return &memory[a];
	}
	else {
		error(line, MEMORY_REF_ERROR);
		return &memory[0];
	}
}
