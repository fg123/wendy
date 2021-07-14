#include "memory.h"
#include "error.h"
#include "global.h"
#include <string.h>
#include <stdio.h>

// Memory.c, provides functions for the interpreter to manipulate memory

#define CHAR(s) (*s)

#define resize(container, current) do { \
	current *= 2; \
	container = safe_realloc(container, current * sizeof(*container)); \
} while(0)

// Because we store return addresses and frame pointers in the stack
//   with the rest of the stack data, we use the following two functions
//   to convert to and from
#define wrap_number(number) \
	make_data(D_NUMBER, data_value_num((number)))

#define unwrap_number(data) \
	(data).value.number

static inline bool is_at_main(struct memory * memory) {
	return memory->call_stack_pointer == 1;
}

void check_memory(struct memory * memory) {
	// Check stack
	if (memory->call_stack_pointer >= memory->call_stack_size - 1) {
		resize(memory->call_stack, memory->call_stack_size);
	}
	// Check argstack
	if (memory->working_stack_pointer >= memory->working_stack_size - 1) {
		resize(memory->working_stack, memory->working_stack_size);
	}
}

void ensure_working_stack_size(struct memory * memory, size_t additional) {
	while (memory->working_stack_pointer + additional >= memory->working_stack_size) {
		resize(memory->working_stack, memory->working_stack_size);
	}
}

struct data *refcnt_malloc_impl(struct memory * memory, void* allocvoid, size_t count) {
	struct data* allocated = (struct data*)allocvoid;
	// We allocate:
	// | refcnt_container | data         |
	// |                  | count * data |
	//                    ^- return ptr to here
	struct refcnt_container* container_info =
		(struct refcnt_container*) allocated;
	container_info->count = count;
	container_info->refs = 1;
	container_info->touched = false;

	if (!memory->all_containers_end && !memory->all_containers_start) {
		memory->all_containers_start = container_info;
		memory->all_containers_end = container_info;
	}

	memory->all_containers_end->next = container_info;
	container_info->prev = memory->all_containers_end;

	memory->all_containers_start->prev = container_info;
	container_info->next = memory->all_containers_start;

	memory->all_containers_end = container_info;
	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("refcnt malloc %p\n", allocated);
	}
	// This forces no pointer arithmetic
	return (struct data*)((unsigned char*)allocated + sizeof(struct refcnt_container));
}

void refcnt_free(struct memory * memory, struct data *ptr) {
	struct refcnt_container* container_info =
		(struct refcnt_container*)((unsigned char*)ptr - sizeof(struct refcnt_container));

	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("refcnt free %p, container %p, count %zd before decrement",
			ptr, container_info, container_info->refs);
		printf("\n");
	}

	if (container_info->refs == 0) {
		error_general("Internal error: refcnt_free on reference with 0 count!");
		safe_exit(1);
		return;
	}

	container_info->refs -= 1;

	if (container_info->refs == 0) {
		if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
			printf("refcnt at 0, looping through %zd to clear\n", container_info->count);
		}
		for (size_t i = 0; i < container_info->count; i++) {
			if (container_info->count < 10 &&
				get_settings_flag(SETTINGS_TRACE_REFCNT)) {
				printf("loop to %p\n", &ptr[i]);
			}
			destroy_data_runtime(memory, &ptr[i]);
		}
		if (container_info == memory->all_containers_start &&
			container_info == memory->all_containers_end) {
			memory->all_containers_start = 0;
			memory->all_containers_end = 0;
		}
		else {
			container_info->prev->next = container_info->next;
			container_info->next->prev = container_info->prev;
			if (container_info == memory->all_containers_end) {
				memory->all_containers_end = container_info->prev;
			}
			if (container_info == memory->all_containers_start) {
				memory->all_containers_start = container_info->next;
			}
			container_info->prev = 0;
			container_info->next = 0;
		}
		safe_free(container_info);
	}
}

struct data *refcnt_copy(struct data *ptr) {
	struct refcnt_container* container_info =
		(struct refcnt_container*)((unsigned char*)ptr - sizeof(struct refcnt_container));
	container_info->refs += 1;
	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("refcnt copy %p, count %zd\n",
			container_info, container_info->refs);
	}
	return ptr;
}


struct data* wendy_list_malloc_impl(void* allocvoid, size_t size) {
	struct data* allocated = (struct data*)allocvoid;
	allocated[0] = list_header_data(size, size);
	return allocated;
}

size_t wendy_list_size(const struct data* list_ref) {
	if (list_ref->type != D_LIST && list_ref->type != D_CLOSURE) {
		error_general("wendy_list_size but not D_LIST");
	}
	struct data* list_data = list_ref->value.reference;
	if (list_data->type != D_LIST_HEADER) {
		error_general("wendy_list_size but not D_LIST_HEADER");
	}
	struct list_header* hdr = (struct list_header*)list_data->value.reference;
	return hdr->size;
}

struct data *create_closure(struct memory * memory) {
	size_t size = 0;
	for (size_t i = 1; i < memory->call_stack_pointer; i++) {
		size += table_size(memory->call_stack[i].variables);
		if (memory->call_stack[i].closure) {
			size += table_size(memory->call_stack[i].closure);
		}
	}
	// We store a closure as a wendy-list of: identifier, value, identifier 2, value 2, etc
	struct data *closure_list = wendy_list_malloc(memory, size * 2);
	size_t index = 1;
	for (size_t i = 1; i < memory->call_stack_pointer; i++) {
		struct table* table = memory->call_stack[i].variables;
		// Iterate Regular Vars
		for (size_t i = 0; i < table->bucket_count; i++) {
			struct entry* curr = table->buckets[i];
			while (curr) {
				closure_list[index++] = make_data(D_IDENTIFIER, data_value_str(curr->key));
				closure_list[index++] = copy_data(curr->value);
				curr = curr->next;
			}
		}
		// Iterate Closure Vars
		table = memory->call_stack[i].closure;
		if (table) {
			for (size_t i = 0; i < table->bucket_count; i++) {
				struct entry* curr = table->buckets[i];
				while (curr) {
					closure_list[index++] = make_data(D_IDENTIFIER, data_value_str(curr->key));
					closure_list[index++] = copy_data(curr->value);
					curr = curr->next;
				}
			}
		}	
	}
	return closure_list;
}

struct memory *memory_init(void) {
	struct memory *memory = malloc(sizeof(*memory));
	
	memory->call_stack_size = INITIAL_STACK_SIZE;
	memory->working_stack_size = INITIAL_WORKING_STACK_SIZE;

	// Initialize Argument Stack
	memory->working_stack = safe_calloc(memory->working_stack_size, sizeof(struct data));

	// Initialize Call Stack
	memory->call_stack = safe_calloc(memory->call_stack_size, sizeof(struct stack_frame));

	memory->working_stack_pointer = 0;
	memory->call_stack_pointer = 0;
	memory->all_containers_start = 0;
	memory->all_containers_end = 0;
	return memory;	
}

void clear_working_stack(struct memory * memory) {
	while (memory->working_stack_pointer > 0) {
		memory->working_stack_pointer--;
		destroy_data_runtime(memory, &memory->working_stack[memory->working_stack_pointer]);
	}
}

address stack_frame_destroy(struct memory* memory, struct stack_frame* frame) {
	table_destroy(memory, frame->variables);
	frame->variables = NULL;
	// May or may not have a closure associated
	if (frame->closure) {
		table_destroy(memory, frame->closure);
		frame->closure = NULL;
	}
	safe_free(frame->fn_name);
	return frame->ret_addr;
}

void memory_destroy(struct memory* memory) {
	for (size_t i = 0; i < memory->working_stack_pointer; i++) {
		destroy_data_runtime(memory, &memory->working_stack[i]);
	}
	safe_free(memory->working_stack);
	for (size_t i = 0; i < memory->call_stack_pointer; i++) {
		stack_frame_destroy(memory, &memory->call_stack[i]);
	}
	safe_free(memory->call_stack);

	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("Call/working stack cleared. Now we clear cycles.\n");
	}

	// Check remaining cycled references
	struct refcnt_container *start = memory->all_containers_start;
	struct refcnt_container *next;

	while (start) {
		// There's no safe way to free the remaining through refcnt_free
		//   due to cycling issues and also freeing a part before
		//   the owning parent has freed, which leads to bad memory
		//   reads. The best way is to forcefully free the remaining
		//   refcnted containers;
		if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
			printf("Clear leftover.\n");
		}
		struct data *ptr = (struct data *)(start + 1);
		for (size_t i = 0; i < start->count; i++) {
			destroy_data_runtime_no_ref(memory, &ptr[i]);
		}
		next = start->next;
		safe_free(start);
		start = next;
		if (start == memory->all_containers_start) {
			break;
		}
	}
	free(memory);
}

void push_frame(struct memory * memory, char* name, address ret, int line) {
	UNUSED(line);
	// Store current frame pointer
	char* se_name = safe_concat(
		name,
		"()"
	);
	struct stack_frame* frame = &memory->call_stack[memory->call_stack_pointer++];
	frame->fn_name = se_name;
	frame->ret_addr = ret;
	frame->variables = table_create();
	frame->closure = table_create();
	frame->is_automatic = false;
}

void push_auto_frame(struct memory * memory, address ret, char* type, int line) {
	UNUSED(line);
	// store current frame pointer
	char* se_name = safe_concat(
		"autoframe:", type, ">"
	);
	struct stack_frame* frame = &memory->call_stack[memory->call_stack_pointer++];
	frame->fn_name = se_name;
	frame->ret_addr = ret;
	frame->variables = table_create();
	frame->closure = 0;
	frame->is_automatic = true;
}

void pop_frame(struct memory * memory, bool is_ret, address* ret) {
	if (is_at_main(memory)) return;
	address trace = memory->call_stack_pointer - 1;
	if (is_ret) {
		// Pop all automatic frames
		while (memory->call_stack[trace].is_automatic) {
			stack_frame_destroy(memory, &memory->call_stack[trace]);
			trace -= 1;
		}
	}
	// trace now points to the actual frame we want to pop.
	if (is_ret) {
		*ret = stack_frame_destroy(memory, &memory->call_stack[trace]);
	}
	else {
		stack_frame_destroy(memory, &memory->call_stack[trace]);
	}
	memory->call_stack_pointer = trace;
}

void print_working_stack(struct memory * memory, FILE* file, int maxlines) {
	fprintf(file, "\n" DIVIDER "\n");
	fprintf(file, "Dump: Working Stack\n");
	int start = memory->working_stack_pointer - maxlines;
	if (start < 0 || maxlines < 0) start = 0;
	for (size_t i = start; i < memory->working_stack_pointer; i++) {
		fprintf(file, "%5zd      [%s -> ", i, data_string[memory->working_stack[i].type]);
		print_data_inline(&memory->working_stack[i], file);
		fprintf(file, "]\n");
	}
	fprintf(file, DIVIDER "\n");
}

void print_call_stack(struct memory * memory, FILE* file, int maxlines) {
	fprintf(file, "\n" DIVIDER "\n");
	fprintf(file, "Dump: Stack Trace\n");
	int start = memory->call_stack_pointer - maxlines;
	if (start < 0 || maxlines < 0) start = 0;
	for (size_t i = start; i < memory->call_stack_pointer; i++) {
		fprintf(file, "Frame %zd (return to 0x%X): %s\n", i, memory->call_stack[i].ret_addr,
			memory->call_stack[i].fn_name);
		if (memory->call_stack[i].closure) {
			table_print(file, memory->call_stack[i].closure,   "  C[%s = ", "]");
		}
		table_print(file, memory->call_stack[i].variables, "   [%s = ", "]");
	}
	fprintf(file, DIVIDER "\n");
}

void push_arg(struct memory * memory, struct data t) {
	memory->working_stack[memory->working_stack_pointer++] = t;
	check_memory(memory);
}

struct data* top_arg(struct memory * memory, int line) {
	if (memory->working_stack_pointer != 0) {
		return &memory->working_stack[memory->working_stack_pointer - 1];
	}
	error_runtime(memory, line, MEMORY_STACK_UNDERFLOW);
	return 0;
}

struct data pop_arg(struct memory * memory, int line) {
	if (memory->working_stack_pointer != 0) {
		struct data ret = memory->working_stack[--(memory->working_stack_pointer)];
		memory->working_stack[memory->working_stack_pointer] = make_data(D_EMPTY, data_value_num(0));
		return ret;
	}
	error_runtime(memory, line, MEMORY_STACK_UNDERFLOW);
	return none_data();
}

struct data* push_stack_entry(struct memory * memory, char* id, int line) {
	// TODO(felixguo): can probably remove line
	UNUSED(line);
	struct data* d = table_insert(
		memory->call_stack[memory->call_stack_pointer - 1].variables, id, memory);
	check_memory(memory);
	return d;
}

struct data* push_closure_entry(struct memory * memory, char* id, int line) {
	// TODO(felixguo): can probably remove line
	UNUSED(line);
	struct data* d = table_insert(
		memory->call_stack[memory->call_stack_pointer - 1].closure, id, memory);
	check_memory(memory);
	return d;
}

bool id_exist(struct memory * memory, char* id, bool search_main) {
	return get_address_of_id(memory, id, search_main, NULL) != NULL;
}

bool id_exist_local_frame_ignore_closure(struct memory* memory, char* id) {
	// Only search local frame
	return table_exist(memory->call_stack[memory->call_stack_pointer - 1].variables, id);
}

struct data* get_address_of_id(struct memory * memory, char* id, bool search_main, bool* is_closure) {
	struct data* result = 0;
	if (is_closure) {
		*is_closure = false;
	}
	size_t trace = memory->call_stack_pointer - 1;
	while (memory->call_stack[trace].is_automatic) {
		// Look through automatics
		result = table_find(memory->call_stack[trace].variables, id);
		if (result) {
			return result;
		}
		trace -= 1;
	}
	// Non automatic
	result = table_find(memory->call_stack[trace].variables, id);
	if (result) {
		return result;
	}
	result = table_find(memory->call_stack[trace].closure, id);
	if (result) {
		if (is_closure) {
			*is_closure = true;
		}
		return result;
	}
	
	// Main frame
	if (search_main) {
		result = table_find(memory->call_stack[0].variables, id);
		if (result) {
			return result;
		}
	}
	return 0;
}

void unwind_stack(struct memory * memory) {
	address pointer;
	while (!is_at_main(memory)) {
		pop_frame(memory, true, &pointer);
	}
}
