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

struct data* working_stack;
struct stack_entry* call_stack;

address frame_pointer = 0;
address working_stack_pointer = 0;
address call_stack_pointer = 0;

size_t call_stack_size = INITIAL_STACK_SIZE;
size_t working_stack_size = INITIAL_WORKING_STACK_SIZE;

struct refcnt_container* all_containers_start = 0;
struct refcnt_container* all_containers_end = 0;

// Pointer to the end of the main() stack frame
static address main_end_pointer = 0;

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

static inline bool is_at_main(void) {
	return frame_pointer == 0;
}

static inline bool is_identifier_entry(int index) {
	return call_stack[index].id[0] != CHAR(FUNCTION_START) &&
		   call_stack[index].id[0] != CHAR(AUTOFRAME_START) &&
		   call_stack[index].id[0] != CHAR(RA_START);
}

void check_memory(void) {
	// Check stack
	if (call_stack_pointer >= call_stack_size - 1) {
		resize(call_stack, call_stack_size);
	}
	// Check argstack
	if (working_stack_pointer >= working_stack_size - 1) {
		resize(working_stack, working_stack_size);
	}
}

void ensure_working_stack_size(size_t additional) {
	while (working_stack_pointer + additional >= working_stack_size) {
		resize(working_stack, working_stack_size);
	}
}

struct data *refcnt_malloc_impl(struct data* allocated, size_t count) {
	// We allocate:
	// | refcnt_container | data         |
	// |                  | count * data |
	//                    ^- return ptr to here
	struct refcnt_container* container_info =
		(struct refcnt_container*) allocated;
	container_info->count = count;
	container_info->refs = 1;
	container_info->touched = false;

	if (!all_containers_end && !all_containers_start) {
		all_containers_start = container_info;
		all_containers_end = container_info;
	}

	all_containers_end->next = container_info;
	container_info->prev = all_containers_end;

	all_containers_start->prev = container_info;
	container_info->next = all_containers_start;

	all_containers_end = container_info;
	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("refcnt malloc %p\n", allocated);
	}
	// This forces no pointer arithmetic
	return (void*)allocated + sizeof(struct refcnt_container);
}

void refcnt_free(struct data *ptr) {
	struct refcnt_container* container_info =
		(void*)ptr - sizeof(struct refcnt_container);

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
			destroy_data(&ptr[i]);
		}
		if (container_info == all_containers_start && container_info == all_containers_end) {
			all_containers_start = 0;
			all_containers_end = 0;
		}
		else {
			container_info->prev->next = container_info->next;
			container_info->next->prev = container_info->prev;
			if (container_info == all_containers_end) {
				all_containers_end = container_info->prev;
			}
			if (container_info == all_containers_start) {
				all_containers_start = container_info->next;
			}
			container_info->prev = 0;
			container_info->next = 0;
		}
		safe_free(container_info);
	}
}

struct data *refcnt_copy(struct data *ptr) {
	struct refcnt_container* container_info =
		(void*)ptr - sizeof(struct refcnt_container);
	container_info->refs += 1;
	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("refcnt copy %p, count %zd\n",
			container_info, container_info->refs);
	}
	return ptr;
}

struct data* wendy_list_malloc_impl(struct data* allocated, size_t size) {
	allocated[0] = make_data(D_LIST_HEADER, data_value_num(size));
	return allocated;
}

struct data *create_closure(void) {
	size_t size = 0;
	for (size_t i = main_end_pointer; i < call_stack_pointer; i++) {
		if (is_identifier_entry(i)) {
			size += 1;
		}
	}
	// We store a closure as a wendy-list of: identifier, value, identifier 2, value 2, etc
	struct data *closure_list = wendy_list_malloc(size * 2);
	size_t index = 1;
	for (size_t i = main_end_pointer; i < call_stack_pointer; i++) {
		if (is_identifier_entry(i)) {
			// Add to closure list
			closure_list[index++] = make_data(D_IDENTIFIER, data_value_str(call_stack[i].id));
			closure_list[index++] = copy_data(call_stack[i].val);
		}
	}
	return closure_list;
}

void init_memory(void) {
	// Initialize Argument Stack
	working_stack = safe_calloc(working_stack_size, sizeof(struct data));

	// Initialize Call Stack
	call_stack = safe_calloc(call_stack_size, sizeof(struct stack_entry));
}

void clear_working_stack(void) {
	while (working_stack_pointer > 0) {
		working_stack_pointer--;
		destroy_data(&working_stack[working_stack_pointer]);
	}
}

void free_memory(void) {
	for (size_t i = 0; i < working_stack_pointer; i++) {
		destroy_data(&working_stack[i]);
	}
	safe_free(working_stack);
	for (size_t i = 0; i < call_stack_pointer; i++) {
		if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
			printf("Clearing %s at %p\n", call_stack[i].id, call_stack[i].val.value.reference);
		}
		destroy_data(&call_stack[i].val);
		safe_free(call_stack[i].id);
	}
	safe_free(call_stack);

	if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
		printf("Call/working stack cleared. Now we clear cycles.\n");
	}

	// Check remaining cycled references
	struct refcnt_container *start = all_containers_start;
	struct refcnt_container *next;

	while (start) {
		// There's no safe way to free the remaining through refcnt_free
		//   due to cycling issues and also freeing a part before
		//   the owning parent has freed, which leads to bad memory
		//   reads. The best way is to forcefully free the remaining
		//   refcnted containers;
		struct data *ptr = (struct data *)(start + 1);
		for (size_t i = 0; i < start->count; i++) {
			if (!is_reference(ptr[i])) {
				// If it's a reference, it will be dealt with later
				//   or before
				destroy_data(&ptr[i]);
			}
		}
		next = start->next;
		safe_free(start);
		start = next;
		if (start == all_containers_start) {
			break;
		}
	}
}

void push_frame(char* name, address ret, int line) {
	// Store current frame pointer
	char* se_name = safe_concat(
		FUNCTION_START " ",
		name,
		"()"
	);
	address old_fp = frame_pointer;
	frame_pointer = call_stack_pointer;

	struct stack_entry* se = push_stack_entry(se_name, line);
	se->val = wrap_number(old_fp);

	safe_free(se_name);

	se = push_stack_entry(RA_START "RET", line);
	se->val = wrap_number(ret);
}

void push_auto_frame(address ret, char* type, int line) {
	// store current frame pointer
	char* se_name = safe_concat(
		AUTOFRAME_START "autoframe:", type, ">"
	);
	address old_fp = frame_pointer;
	frame_pointer = call_stack_pointer;

	struct stack_entry* se = push_stack_entry(se_name, line);
	se->val = wrap_number(old_fp);

	safe_free(se_name);

	se = push_stack_entry(RA_START "RET", line);
	se->val = wrap_number(ret);
}

bool pop_frame(bool is_ret, address* ret) {
	if (is_at_main()) return true;
	address trace = frame_pointer;
	if (is_ret) {
		while (call_stack[trace].id[0] != CHAR(FUNCTION_START)) {
			trace = unwrap_number(call_stack[trace].val);
		}
		*ret = unwrap_number(call_stack[trace + 1].val);
	}
	frame_pointer = unwrap_number(call_stack[trace].val);
	bool is_at_function = call_stack[trace].id[0] == CHAR(FUNCTION_START);

	while (call_stack_pointer > trace) {
		call_stack_pointer -= 1;
		safe_free(call_stack[call_stack_pointer].id);
		destroy_data(&call_stack[call_stack_pointer].val);
	}
	return (is_ret || is_at_function);
}

void print_call_stack(FILE* file, int maxlines) {
	fprintf(file, "\n" DIVIDER "\n");
	fprintf(file, "Dump: Stack Trace\n");
	int start = call_stack_pointer - maxlines;
	if (start < 0 || maxlines < 0) start = 0;
	for (size_t i = start; i < call_stack_pointer; i++) {
		if (call_stack[i].id[0] != '$' && call_stack[i].id[0] != '~') {
			if (frame_pointer == i) {
				if (call_stack[i].id[0] == CHAR(FUNCTION_START)) {
					fprintf(file, "%5zd FP-> [" BLU "%s" RESET " -> ", i,
							call_stack[i].id);
				}
				else {
					fprintf(file, "%5zd FP-> [%s -> ", i, call_stack[i].id);
				}
				fprintf(file, "]\n");
			}
			else {
				if (call_stack[i].id[0] == CHAR(FUNCTION_START)) {
					fprintf(file, "%5zd      [" BLU "%s" RESET " -> ", i,
							call_stack[i].id);
				}
				else if (call_stack[i].is_closure) {
					fprintf(file, "%5zd  C-> [%s -> ",i,
							call_stack[i].id);
				}
				else {
					fprintf(file, "%5zd      [%s -> ",i,
							call_stack[i].id);
				}
				print_data_inline(&call_stack[i].val, stdout);
				fprintf(file, "]\n");
			}
		}
	}
	fprintf(file, DIVIDER "\n");
}

void push_arg(struct data t) {
	working_stack[working_stack_pointer++] = t;
	check_memory();
}

struct data* top_arg(int line) {
	if (working_stack_pointer != 0) {
		return &working_stack[working_stack_pointer - 1];
	}
	error_runtime(line, MEMORY_STACK_UNDERFLOW);
	return 0;
}

struct data pop_arg(int line) {
	if (working_stack_pointer != 0) {
		struct data ret = working_stack[--working_stack_pointer];
		working_stack[working_stack_pointer] = make_data(D_EMPTY, data_value_num(0));
		return ret;
	}
	error_runtime(line, MEMORY_STACK_UNDERFLOW);
	return none_data();
}

struct stack_entry* push_stack_entry(char* id, int line) {
	// TODO(felixguo): can probably remove line
	UNUSED(line);
	struct stack_entry new_entry;
	new_entry.is_closure = false;
	new_entry.id = safe_strdup(id);

	// We cannot be sure that .val will be written to, so we cannot
	//   leave it uninitialized.
	new_entry.val = make_data(D_EMPTY, data_value_num(0));
	call_stack[call_stack_pointer++] = new_entry;
	if (is_at_main()) {
		// currently in main function
		main_end_pointer = call_stack_pointer;
	}
	check_memory();
	return &call_stack[call_stack_pointer - 1];
}

address get_fn_frame_ptr(void) {
	address trace = frame_pointer;
	while (call_stack[trace].id[0] != CHAR(FUNCTION_START)) {
		trace = unwrap_number(call_stack[trace].val);
	}
	return trace;
}

bool id_exist(char* id, bool search_main) {
	size_t start = frame_pointer;
	if (search_main) {
		start = get_fn_frame_ptr();
	}
	for (size_t i = start + 1; i < call_stack_pointer; i++) {
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

struct data* get_address_of_id(char* id, int line) {
	address stack_entry = get_stack_pos_of_id(id, line);
	if (!stack_entry) {
		/* Some error occured, nothing lies at 0 */
		return 0;
	}
	return &call_stack[stack_entry].val;
}

address get_stack_pos_of_id(char* id, int line) {
	if (!id_exist(id, true)) {
		error_runtime(line, MEMORY_ID_NOT_FOUND, id);
		return 0;
	}
	address frame_ptr = get_fn_frame_ptr();
	for (size_t i = call_stack_pointer - 1; i > frame_ptr; i--) {
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

struct data* get_value_of_id(char* id, int line) {
	// return get_value_of_address(get_address_of_id(id, line), line);
	return get_address_of_id(id, line);
}

struct data* get_value_of_address(struct data* a, int line) {
	// TODO(felixguo): This is wrong
	UNUSED(a);
	UNUSED(line);
	return NULL;
}

void unwind_stack(void) {
	address pointer;
	while (!is_at_main()) {
		pop_frame(true, &pointer);
	}
}
