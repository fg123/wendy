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
	return memory->frame_pointer == 0;
}

static inline bool is_identifier_entry(struct memory * memory, int index) {
	return memory->call_stack[index].id[0] != CHAR(FUNCTION_START) &&
		   memory->call_stack[index].id[0] != CHAR(AUTOFRAME_START) &&
		   memory->call_stack[index].id[0] != CHAR(RA_START);
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

struct data *refcnt_malloc_impl(struct memory * memory, struct data* allocated, size_t count) {
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


struct data* wendy_list_malloc_impl(struct data* allocated, size_t size) {
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
	for (size_t i = memory->main_end_pointer; i < memory->call_stack_pointer; i++) {
		if (is_identifier_entry(memory, i)) {
			size += 1;
		}
	}
	// We store a closure as a wendy-list of: identifier, value, identifier 2, value 2, etc
	struct data *closure_list = wendy_list_malloc(memory, size * 2);
	size_t index = 1;
	for (size_t i = memory->main_end_pointer; i < memory->call_stack_pointer; i++) {
		if (is_identifier_entry(memory, i)) {
			// Add to closure list
			closure_list[index++] = make_data(D_IDENTIFIER, data_value_str(memory->call_stack[i].id));
			closure_list[index++] = copy_data(memory->call_stack[i].val);
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
	memory->call_stack = safe_calloc(memory->call_stack_size, sizeof(struct stack_entry));

	memory->frame_pointer = 0;
	memory->working_stack_pointer = 0;
	memory->call_stack_pointer = 0;
	memory->all_containers_start = 0;
	memory->all_containers_end = 0;
	memory->main_end_pointer = 0;
	return memory;	
}

void clear_working_stack(struct memory * memory) {
	while (memory->working_stack_pointer > 0) {
		memory->working_stack_pointer--;
		destroy_data_runtime(memory, &memory->working_stack[memory->working_stack_pointer]);
	}
}

void memory_destroy(struct memory* memory) {
	for (size_t i = 0; i < memory->working_stack_pointer; i++) {
		destroy_data_runtime(memory, &memory->working_stack[i]);
	}
	safe_free(memory->working_stack);
	for (size_t i = 0; i < memory->call_stack_pointer; i++) {
		if (get_settings_flag(SETTINGS_TRACE_REFCNT)) {
			printf("Clearing %s at %p\n", memory->call_stack[i].id,
				memory->call_stack[i].val.value.reference);
		}
		destroy_data_runtime(memory, &memory->call_stack[i].val);
		safe_free(memory->call_stack[i].id);
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
	// Store current frame pointer
	char* se_name = safe_concat(
		FUNCTION_START " ",
		name,
		"()"
	);
	address old_fp = memory->frame_pointer;
	memory->frame_pointer = memory->call_stack_pointer;

	struct stack_entry* se = push_stack_entry(memory, se_name, line);
	se->val = wrap_number(old_fp);

	safe_free(se_name);

	se = push_stack_entry(memory, RA_START "RET", line);
	se->val = wrap_number(ret);
}

void push_auto_frame(struct memory * memory, address ret, char* type, int line) {
	// store current frame pointer
	char* se_name = safe_concat(
		AUTOFRAME_START "autoframe:", type, ">"
	);
	address old_fp = memory->frame_pointer;
	memory->frame_pointer = memory->call_stack_pointer;

	struct stack_entry* se = push_stack_entry(memory, se_name, line);
	se->val = wrap_number(old_fp);

	safe_free(se_name);

	se = push_stack_entry(memory, RA_START "RET", line);
	se->val = wrap_number(ret);
}

bool pop_frame(struct memory * memory, bool is_ret, address* ret) {
	if (is_at_main(memory)) return true;
	address trace = memory->frame_pointer;
	if (is_ret) {
		while (memory->call_stack[trace].id[0] != CHAR(FUNCTION_START)) {
			trace = unwrap_number(memory->call_stack[trace].val);
		}
		*ret = unwrap_number(memory->call_stack[trace + 1].val);
	}
	memory->frame_pointer = unwrap_number(memory->call_stack[trace].val);
	bool is_at_function = memory->call_stack[trace].id[0] == CHAR(FUNCTION_START);

	while (memory->call_stack_pointer > trace) {
		memory->call_stack_pointer -= 1;
		safe_free(memory->call_stack[memory->call_stack_pointer].id);
		destroy_data_runtime(memory, &memory->call_stack[memory->call_stack_pointer].val);
	}
	return (is_ret || is_at_function);
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
		if (memory->call_stack[i].id[0] != '$' && memory->call_stack[i].id[0] != '~') {
			if (memory->frame_pointer == i) {
				if (memory->call_stack[i].id[0] == CHAR(FUNCTION_START)) {
					fprintf(file, "%5zd FP-> [" BLU "%s" RESET " -> ", i,
							memory->call_stack[i].id);
				}
				else {
					fprintf(file, "%5zd FP-> [%s -> ", i, memory->call_stack[i].id);
				}
				fprintf(file, "]\n");
			}
			else {
				if (memory->call_stack[i].id[0] == CHAR(FUNCTION_START)) {
					fprintf(file, "%5zd      [" BLU "%s" RESET " -> ", i,
							memory->call_stack[i].id);
				}
				else if (memory->call_stack[i].is_closure) {
					fprintf(file, "%5zd  C-> [%s -> ",i,
							memory->call_stack[i].id);
				}
				else {
					fprintf(file, "%5zd      [%s -> ",i,
							memory->call_stack[i].id);
				}
				print_data_inline(&memory->call_stack[i].val, file);
				fprintf(file, "]\n");
			}
		}
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

struct stack_entry* push_stack_entry(struct memory * memory, char* id, int line) {
	// TODO(felixguo): can probably remove line
	UNUSED(line);
	struct stack_entry new_entry;
	new_entry.is_closure = false;
	new_entry.id = safe_strdup(id);

	// We cannot be sure that .val will be written to, so we cannot
	//   leave it uninitialized.
	new_entry.val = make_data(D_EMPTY, data_value_num(0));
	memory->call_stack[memory->call_stack_pointer++] = new_entry;
	if (is_at_main(memory)) {
		// currently in main function
		memory->main_end_pointer = memory->call_stack_pointer;
	}
	check_memory(memory);
	return &memory->call_stack[memory->call_stack_pointer - 1];
}

address get_fn_frame_ptr(struct memory * memory) {
	address trace = memory->frame_pointer;
	while (memory->call_stack[trace].id[0] != CHAR(FUNCTION_START)) {
		trace = unwrap_number(memory->call_stack[trace].val);
	}
	return trace;
}

bool id_exist(struct memory * memory, char* id, bool search_main) {
	size_t start = memory->frame_pointer;
	if (search_main) {
		start = get_fn_frame_ptr(memory);
	}
	for (size_t i = start + 1; i < memory->call_stack_pointer; i++) {
		if (streq(id, memory->call_stack[i].id)) {
			return true;
		}
	}
	if (search_main) {
		for (size_t i = 0; i < memory->main_end_pointer; i++) {
			if (streq(id, memory->call_stack[i].id)) {
				return true;
			}
		}
	}
	return false;
}

struct data* get_address_of_id(struct memory * memory, char* id, int line) {
	address stack_entry = get_stack_pos_of_id(memory, id, line);
	if (!stack_entry) {
		/* Some error occured, nothing lies at 0 */
		return 0;
	}
	return &memory->call_stack[stack_entry].val;
}

address get_stack_pos_of_id(struct memory * memory, char* id, int line) {
	if (!id_exist(memory, id, true)) {
		error_runtime(memory, line, MEMORY_ID_NOT_FOUND, id);
		return 0;
	}
	address frame_ptr = get_fn_frame_ptr(memory);
	for (size_t i = memory->call_stack_pointer - 1; i > frame_ptr; i--) {
		if (streq(id, memory->call_stack[i].id)) {
			return i;
		}
	}
	for (size_t i = 0; i < memory->main_end_pointer; i++) {
		if (streq(id, memory->call_stack[i].id)) {
			return i;
		}
	}
	return 0;
}

struct data* get_value_of_id(struct memory * memory, char* id, int line) {
	// return get_value_of_address(get_address_of_id(id, line), line);
	return get_address_of_id(memory, id, line);
}

void unwind_stack(struct memory * memory) {
	address pointer;
	while (!is_at_main(memory)) {
		pop_frame(memory, true, &pointer);
	}
}
