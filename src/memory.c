#include "memory.h"
#include "error.h"
#include "global.h"
#include <string.h>
#include <stdio.h>

// Memory.c, provides functions for the interpreter to manipulate memory

#define FUNCTION_START ">"
#define AUTOFRAME_START "<"
#define RA_START "#"
#define CHAR(s) (*s)

data* memory;
data* arg_stack;
mem_block* free_memory;
stack_entry* call_stack;
address* mem_reg_stack;
stack_entry** closure_list;
size_t* closure_list_sizes;

address frame_pointer = 0;
address stack_pointer = 0;
address arg_pointer = 0;
address closure_list_pointer = 0;

size_t closure_list_size = 0;
address mem_reg_pointer = 0;

size_t callstack_size = INITIAL_STACK_SIZE;
size_t argstack_size = INITIAL_ARGSTACK_SIZE;
size_t memregstack_size = INITIAL_MEMREGSTACK_SIZE;

// Pointer to the end of the main() stack frame
static address main_end_pointer = 0;

#define resize(container, current) do { \
	current *= 2; \
	container = safe_realloc(container, current * sizeof(*container)); \
} while(0)

static inline bool is_at_main(void) {
	return frame_pointer == 0;
}
static inline bool is_identifier_entry(int index) {
	return call_stack[index].id[0] != CHAR(FUNCTION_START) &&
		   call_stack[index].id[0] != CHAR(AUTOFRAME_START) &&
		   call_stack[index].id[0] != CHAR(RA_START);
}

void write_memory(unsigned int location, data d, int line) {
	if (location < MEMORY_SIZE) {
		destroy_data(memory + location);
		memory[location] = d;
	}
	else {
		error_runtime(line, MEMORY_REF_ERROR);
	}
}

void mark_locations(bool* marked, address start, size_t block_size) {
	for (size_t j = start; j < start + block_size; j++) {
		marked[j] = true;
	}
}

bool garbage_collect(size_t size) {
	if (get_settings_flag(SETTINGS_NOGC)) {
		return has_memory(size);
	}
	// Garbage! We'll implement the most basic mark and sweep algo.
	bool *marked = safe_calloc(MEMORY_SIZE, sizeof(bool));
	for (size_t i = 0; i < RESERVED_MEMORY; i++) {
		// Don't delete the reserved ones!
		marked[i] = true;
	}
	for (size_t i = 0; i < stack_pointer; i++) {
		if (is_identifier_entry(i)) {
			address a = call_stack[i].val;
			// Mark it!
			marked[a] = true;
			// Check if it's a pointer type?
			if (memory[a].type == D_LIST || memory[a].type == D_STRUCT) {
				a = memory[a].value.number;
				mark_locations(marked, a, memory[a].value.number);
			}
			else if (memory[a].type == D_FUNCTION) {
				a = memory[a].value.number;
				mark_locations(marked, a, 3);
			}
			else if (memory[a].type == D_STRUCT_INSTANCE) {
				a = memory[a].value.number;
				// a points to D_STRUCT_INSTANCE_HEAD
				address meta_loc = memory[a].value.number;
				// meta_loc points to D_STRUCT_METADATA, mark the metadata
				size_t meta_size = memory[meta_loc].value.number;
				mark_locations(marked, meta_loc, meta_size);
				// count parameters
				size_t params = 0;
				for (address i = meta_loc; i < meta_loc + meta_size; i++) {
					if (memory[i].type == D_STRUCT_PARAM) {
						params++;
					}
				}
				// Mark parameters, +1 for T_STRUCT_INSTANCE_HEAD
				mark_locations(marked, a, params + 1);
			}
		}
	}

	for (size_t i = 0; i < MEMORY_SIZE; i++) {
		if (!marked[i]) {
			here_u_go(i, 1);
		}
	}
	safe_free(marked);
	return has_memory(size);
}

address create_closure(void) {
	address location = closure_list_pointer;
	// Things to reserve.
	size_t size = stack_pointer - main_end_pointer;

	stack_entry* closure = safe_malloc(sizeof(stack_entry) * size);
	size_t actual_size = 0;
	for (size_t i = main_end_pointer; i < stack_pointer; i++) {
		if (is_identifier_entry(i)) {
			strcpy(closure[actual_size].id, call_stack[i].id);
			closure[actual_size].val = call_stack[i].val;
			actual_size++;
		}
	}
	if (actual_size <= 0) {
		safe_free(closure);
		return NO_CLOSURE;
	}
	closure = safe_realloc(closure, actual_size * sizeof(stack_entry));
	closure_list[closure_list_pointer] = closure;
	closure_list_sizes[closure_list_pointer] = actual_size;
	closure_list_pointer++;

	if (closure_list_pointer == closure_list_size) {
		// Resize for more storage
		closure_list_size *= 2;
		closure_list = safe_realloc(closure_list,
			closure_list_size * sizeof(stack_entry*));
		// Reset 0s
		for (size_t i = closure_list_pointer; i < closure_list_size; i++) {
			closure_list[i] = 0;
		}
		closure_list_sizes = safe_realloc(closure_list_sizes,
			closure_list_size * sizeof(size_t));
	}
	return location;
}

bool has_memory(size_t size) {
	mem_block* c = free_memory;
	mem_block** p = &free_memory;
	while (c) {
		if (c->size >= size) {
			return true;
		}
		else if (c->size == 0) {
			(*p) = c->next;
			safe_free(c);
		}
		else {
			p = &c->next;
		}
		c = *p;
	}
	return false;
}

address pls_give_memory(size_t size, int line) {
	if (has_memory(size)) {
		mem_block* c = free_memory;
		while (c) {
			if (c->size >= size) {
				// Enough Room!
				c->size -= size;
				address start = c->start;
				c->start += size;
				//print_free_memory();
				return start;
			}
			else {
				// Try next block...
				c = c->next;
			}
		}
	}
	// No free memory blocks???
	if (garbage_collect(size)) {
		return pls_give_memory(size, line);
	}
	else {
		error_runtime(line, MEMORY_OVERFLOW);
		return 0;
	}
}

void here_u_go(address a, size_t size) {
	// Returned memory! Yay!
	// Returned memory could also be already freed.
	// We'll look to see if the returned memory can be appended to another
	//   free block. If not, then we append to the front. Memory could be
	//   in front of an existing or behind.
	mem_block *c = free_memory;
	address end = a + size;
	while (c) {
		if (a >= c->start && a + size <= c->start + c->size) {
			// Block was already freed!
			return;
		}
		else if (c->start == end) {
			// New block slides in front of another.
			c->start = a;
			c->size += size;
			return;
		}
		else if (c->start + c->size == a) {
			// New block slides behind another.
			c->size += size;
			return;
		}
		else {
			// Try next block.
			c = c->next;
		}
	}
	// No other spot, we'll stick it to the front of the free_memory list!
	mem_block* new_m_block = safe_malloc(sizeof(mem_block));
	new_m_block->start = a;
	new_m_block->size = size;
	new_m_block->next = free_memory;

	free_memory = new_m_block;
}


void print_free_memory(void) {
	printf("=============\n");
	printf("Free Memory Blocks:\n");
	mem_block* c = free_memory;
	while(c) {
		printf("Block from %d(0x%X) with size %zd.\n", c->start, c->start, c->size);
		c = c->next;
	}
	printf("=============\n");
}

void init_memory(void) {
	// Initialize Memory
	memory = safe_calloc(MEMORY_SIZE, sizeof(data));

	// Initialize Argument Stack
	arg_stack = safe_calloc(argstack_size, sizeof(data));

	// Initialize MemReg
	mem_reg_stack = safe_calloc(memregstack_size, sizeof(address));

	// Initialize linked list of Free Memory
	free_memory = safe_malloc(sizeof(mem_block));
	free_memory->size = MEMORY_SIZE - RESERVED_MEMORY;
	free_memory->start = RESERVED_MEMORY;
	free_memory->next = 0;

	// Initialize Call Stack
	call_stack = safe_calloc(callstack_size, sizeof(stack_entry));

	closure_list = safe_calloc(INITIAL_CLOSURES_SIZE, sizeof(stack_entry*));
	closure_list_sizes = safe_malloc(sizeof(size_t) * INITIAL_CLOSURES_SIZE);
	closure_list_size = INITIAL_CLOSURES_SIZE;

	// ADDRESS 0 REFERS TO NONE_data
	write_memory(0, none_data(), -1);
	// ADDRESS 1 REFERS TO EMPTY RETURNS data
	write_memory(1, noneret_data(), -1);
}

void clear_arg_stack(void) {
	while (arg_pointer --> 0) {
		destroy_data(arg_stack + arg_pointer);
	}
}

void c_free_memory(void) {
	for (size_t i = 0; i < MEMORY_SIZE; i++) {
		destroy_data(memory + i);
	}
	safe_free(memory);
	safe_free(arg_stack);
	safe_free(call_stack);
	safe_free(mem_reg_stack);

	// Clear all the free_memory blocks.
	mem_block* c = free_memory;
	while (c) {
		mem_block* next = c->next;
		safe_free(c);
		c = next;
	}
	safe_free(closure_list_sizes);
	int i = 0;
	while (closure_list[i]) {
		safe_free(closure_list[i]);
		i++;
	}
	safe_free(closure_list);
}

void check_memory(int line) {
	// Check stack
	if (stack_pointer >= callstack_size - 10) {
		resize(call_stack, callstack_size);
		// printf("Call stack at %d with limit %d!", stack_pointer, STACK_SIZE);
		// error_runtime(line, MEMORY_STACK_OVERFLOW);
	}
	// Check argstack
	if (arg_pointer >= argstack_size - 10) {
		resize(arg_stack, argstack_size);
		// printf("Internal Stack out of memory! %d with limit %d.\n",
		// 		arg_pointer, ARGSTACK_SIZE);
		// error_runtime(line, MEMORY_STACK_OVERFLOW);
	}
	if (!has_memory(1)) {
		// Collect Garbage
		if (garbage_collect(1)) {
			check_memory(line);
		}
		else {
			printf("Out of memory with limit %d!\n", MEMORY_SIZE);
			error_runtime(line, MEMORY_OVERFLOW);
		}
	}
}

void push_frame(char* name, address ret, int line) {
	// store current frame pointer
	stack_entry new_entry = { FUNCTION_START " ", frame_pointer, false };
	strcat(new_entry.id, name);
	strcat(new_entry.id, "()");
	// this new entry will be stored @ location stack_pointer, so we move the
	//   frame pointer to this location
	frame_pointer = stack_pointer;
	// add and increment stack_pointer
	call_stack[stack_pointer++] = new_entry;
	check_memory(line);
	stack_entry ne2 = { RA_START "RET", ret, false };
	call_stack[stack_pointer++] = ne2;
	check_memory(line);
}

void push_auto_frame(address ret, char* type, int line) {
	// store current frame pointer
	stack_entry new_entry = { AUTOFRAME_START "autoframe:", frame_pointer, false };
	strcat(new_entry.id, type);
	strcat(new_entry.id, ">");

	frame_pointer = stack_pointer;
	call_stack[stack_pointer++] = new_entry;

	check_memory(line);

	stack_entry ne2 = { RA_START "RET", ret, false };
	call_stack[stack_pointer++] = ne2;
	check_memory(line);
}

bool pop_frame(bool is_ret, address* ret) {
	if (is_at_main()) return true;
	address trace = frame_pointer;
	if (is_ret) {
		while (call_stack[trace].id[0] != CHAR(FUNCTION_START)) {
			trace = call_stack[trace].val;
            pop_mem_reg();
		}
		*ret = call_stack[trace + 1].val;
	}
	stack_pointer = trace;
	frame_pointer = call_stack[trace].val;
	return (is_ret || call_stack[trace].id[0] == CHAR(FUNCTION_START));
}

void write_state(FILE* fp) {
	fprintf(fp, "FramePointer: %d\n", frame_pointer);
	fprintf(fp, "StackTrace: %d\n", stack_pointer);
	for (size_t i = 0; i < stack_pointer; i++) {
		if (call_stack[i].id[0] == CHAR(FUNCTION_START)) {
			fprintf(fp, ">%s %d\n", call_stack[i].id, call_stack[i].val);
		}
		else {
			fprintf(fp, "%s %d\n", call_stack[i].id, call_stack[i].val);
		}
	}
	fprintf(fp, "Memory: %d\n", MEMORY_SIZE);
	for (size_t i = 0; i < MEMORY_SIZE; i++) {
		if (memory[i].type != 0) {
			fprintf(fp, "%zd %s ", i, data_string[memory[i].type]);
			print_data_inline(&memory[i], fp);
			fprintf(fp, "\n");
		}
	}
}

void print_call_stack(FILE* file, int maxlines) {
	fprintf(file, "\n" DIVIDER "\n");
	fprintf(file, "Dump: Stack Trace\n");
	int start = stack_pointer - maxlines;
	if (start < 0 || maxlines < 0) start = 0;
	for (size_t i = start; i < stack_pointer; i++) {
		if (call_stack[i].id[0] != '$' && call_stack[i].id[0] != '~') {
			if (frame_pointer == i) {
				if (call_stack[i].id[0] == CHAR(FUNCTION_START)) {
					fprintf(file, "%5zd FP-> [" BLU "%s" RESET " -> 0x%04X", i,
							call_stack[i].id, call_stack[i].val);
				}
				else {
					fprintf(file, "%5zd FP-> [%s -> 0x%04X", i, call_stack[i].id,
							call_stack[i].val);
				}
				fprintf(file, "]\n");
			}
			else {
				if (call_stack[i].id[0] == CHAR(FUNCTION_START)) {
					fprintf(file, "%5zd      [" BLU "%s" RESET " -> 0x%04X: ", i,
							call_stack[i].id, call_stack[i].val);
				}
				else if (call_stack[i].is_closure) {
					fprintf(file, "%5zd  C-> [%s -> 0x%04X: ",i,
							call_stack[i].id, call_stack[i].val);
				}
				else {
					fprintf(file, "%5zd      [%s -> 0x%04X: ",i,
							call_stack[i].id, call_stack[i].val);
				}
				print_data_inline(&memory[call_stack[i].val], stdout);
				fprintf(file, "]\n");

			}
		}
	}
	fprintf(file, DIVIDER "\n");
}

void push_arg(data t, int line) {
	arg_stack[arg_pointer++] = t;
	check_memory(line);
}

data* top_arg(int line) {
	if (arg_pointer != 0) {
		return &arg_stack[arg_pointer - 1];
	}
	error_runtime(line, MEMORY_STACK_UNDERFLOW);
	return 0;
}

data pop_arg(int line) {
	if (arg_pointer != 0) {
		data ret = copy_data(arg_stack[--arg_pointer]);
		destroy_data(&arg_stack[arg_pointer]);
		return ret;
	}
	error_runtime(line, MEMORY_STACK_UNDERFLOW);
	return none_data();
}

address push_memory_array(data* a, int size, int line) {
	address loc = pls_give_memory((size_t)size, line);
	for (int i = 0; i < size; i++) {
		write_memory(loc + i, a[i], line);
	}
	check_memory(line);
	return loc;
}

address push_memory_wendy_list(data* a, int size, int line) {
	address loc = pls_give_memory((size_t)(size + 1), line);
	write_memory(loc, list_header_data(size), line);
	for (int i = 0; i < size; i++) {
		write_memory(loc + i + 1, a[i], line);
	}
	check_memory(line);
	return loc;
}

address push_memory(data t, int line) {
	address loc = pls_give_memory(1, line);
	write_memory(loc, t, line);
	check_memory(line);
	return loc;
}

void copy_stack_entry(stack_entry se, int line) {
	call_stack[stack_pointer] = se;
	call_stack[stack_pointer].is_closure = true;
	stack_pointer++;
	if (is_at_main()) {
		main_end_pointer = stack_pointer;
	}
	check_memory(line);
}

void push_stack_entry(char* id, address val, int line) {
	stack_entry new_entry;
	new_entry.val = val;
	new_entry.is_closure = false;
	strncpy(new_entry.id, id, MAX_IDENTIFIER_LEN);
	new_entry.id[MAX_IDENTIFIER_LEN] = 0; // null term
	call_stack[stack_pointer++] = new_entry;
	if (is_at_main()) {
		// currently in main function
		main_end_pointer = stack_pointer;
	}
	check_memory(line);
}

address get_fn_frame_ptr(void) {
	address trace = frame_pointer;
	while (call_stack[trace].id[0] != CHAR(FUNCTION_START)) {
		trace = call_stack[trace].val;
	}
	return trace;
}

bool id_exist(char* id, bool search_main) {
	size_t start = frame_pointer;
	if (search_main) {
		start = get_fn_frame_ptr();
	}
	for (size_t i = start + 1; i < stack_pointer; i++) {
		if (streq(id, call_stack[i].id)) {
			return true;
		}
	}
	if (search_main) {
		for (size_t i = 0; i < main_end_pointer; i++) {
			if (streq(id, call_stack[i].id)) {
				return true;
			}
		}
	}
	return false;
}

address get_address_of_id(char* id, int line) {
	return call_stack[get_stack_pos_of_id(id, line)].val;
}

address get_stack_pos_of_id(char* id, int line) {
	if (!id_exist(id, true)) {
		error_runtime(line, MEMORY_ID_NOT_FOUND, id);
		return 0;
	}
	address frame_ptr = get_fn_frame_ptr();
	for (size_t i = stack_pointer - 1; i > frame_ptr; i--) {
		if (streq(id, call_stack[i].id)) {
			return i;
		}
	}
	for (size_t i = 0; i < main_end_pointer; i++) {
		if (streq(id, call_stack[i].id)) {
			return i;
		}
	}
	return 0;
}

data* get_value_of_id(char* id, int line) {
	return get_value_of_address(get_address_of_id(id, line), line);
}

data* get_value_of_address(address a, int line) {
	if (a < MEMORY_SIZE) {
		return &memory[a];
	}
	else {
		error_runtime(line, MEMORY_REF_ERROR);
		return &memory[0];
	}
}

void push_mem_reg(address memory_register, int line) {
	UNUSED(line);
	if (mem_reg_pointer >= memregstack_size) {
		resize(mem_reg_stack, memregstack_size);
		// error_runtime(line, MEMORY_REGISTER_STACK_OVERFLOW);
	}
	mem_reg_stack[mem_reg_pointer++] = memory_register;
}

address pop_mem_reg(void) {
	if (mem_reg_pointer == 0) {
		error_general(MEMORY_MEM_STACK_ERROR);
		return 0;
	}
	return mem_reg_stack[--mem_reg_pointer];
}

void unwind_stack(void) {
	address pointer;
	while (!is_at_main()) {
		pop_frame(true, &pointer);
	}
}
