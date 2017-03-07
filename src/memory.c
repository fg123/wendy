#include "memory.h"
#include <string.h>
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

// Memory.c, provides functions for the interpreter to manipulate the local 
//   WendyScript memory

address frame_pointer = 0;
address stack_pointer = 0;
address memory_pointer = 0;

static const char FUNCTION_START_CHAR = '>';
// pointer to the end of the main frame
address main_end_pointer = 0; 

void push_frame(char* name) {
	// store current frame pointer
	stack_entry new_entry = { "> ", frame_pointer };
	strcat(new_entry.id, name);
	strcat(new_entry.id, "()");
	// this new entry will be stored @ location stack_pointer, so we move the 
	//   frame pointer to this location
	frame_pointer = stack_pointer;
	// add and increment stack_pointer
	call_stack[stack_pointer++] = new_entry;
	if (stack_pointer >= STACK_SIZE) {
		error(0, STACK_OVERFLOW); 
	} 
}

void push_auto_frame() {
	// store current frame pointer
	stack_entry new_entry = { "<auto_frame>", frame_pointer };
	frame_pointer = stack_pointer;
	call_stack[stack_pointer++] = new_entry;
	if (stack_pointer >= STACK_SIZE) {
		error(0, STACK_OVERFLOW); 
	} 
}

bool pop_frame(bool is_ret) {
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
	stack_pointer = trace;
	frame_pointer = call_stack[trace].val;

	// function ret or popped a function
	return (is_ret || call_stack[trace].id[0] == FUNCTION_START_CHAR);
}

void print_call_stack() {
	printf("\nDump: Stack Trace\n");
	for (int i = 0; i < stack_pointer; i++) {
//		if (call_stack[i].id[0] == FUNCTION_START_CHAR) {
			if (frame_pointer == i) {
				printf("FP -> [%s: ", call_stack[i].id);
				print_token_inline(&memory[call_stack[i].val]);
				printf("]\n");
			}
			else {

				printf("      [%s: ", call_stack[i].id);
				print_token_inline(&memory[call_stack[i].val]);
				printf("]\n");
			}
//		}
	}
	printf("===============\n");
}

address push_memory(token t) {
	if(memory_pointer >= MEMORY_SIZE) {
		printf("OUT OF MEMORY!\nMemory size: %d\n", memory_pointer);
		exit(1);
	}
	memory[memory_pointer++] = t;
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
	if (stack_pointer >= STACK_SIZE) {
		error(0, STACK_OVERFLOW); 
	} 
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
