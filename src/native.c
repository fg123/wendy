#include "native.h"
#include "memory.h"
#include "error.h"
#include "data.h"
#include "codegen.h"
#include "vm.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

#ifdef __linux__
#include <pthread.h>
#endif

char** program_arguments = 0;
int program_arguments_count = 0;
struct native_function {
	const char* name;
	int argc;
	struct data (*function)(struct vm*, struct data*);
};

static struct data native_printCallStack(struct vm* vm, struct data* args);
static struct data native_reverseString(struct vm* vm, struct data* args);
static struct data native_stringToInteger(struct vm* vm, struct data* args);
static struct data native_exec(struct vm* vm, struct data* args);
static struct data native_getc(struct vm* vm, struct data* args);
static struct data native_printBytecode(struct vm* vm, struct data* args);
static struct data native_getImportedLibraries(struct vm* vm, struct data* args);
static struct data native_getProgramArgs(struct vm* vm, struct data* args);
static struct data native_read(struct vm* vm, struct data* args);
static struct data native_readRaw(struct vm* vm, struct data* args);
static struct data native_readFile(struct vm* vm, struct data* args);
static struct data native_writeFile(struct vm* vm, struct data* args);
static struct data native_vm_getRefs(struct vm* vm, struct data* args);
static struct data native_vm_getAt(struct vm* vm, struct data* args);
static struct data native_process_execute(struct vm* vm, struct data* args);
static struct data native_pow(struct vm* vm, struct data* args);
static struct data native_ln(struct vm* vm, struct data* args);
static struct data native_log(struct vm* vm, struct data* args);
static struct data native_random_float(struct vm* vm, struct data* args);

static struct data native_dispatch(struct vm* vm, struct data* args);

static struct native_function native_functions[256] = {
	{ "printCallStack", 1, native_printCallStack },
	{ "reverseString", 1, native_reverseString },
	{ "stringToInteger", 1, native_stringToInteger },
	{ "exec", 1, native_exec },
	{ "getc", 0, native_getc },
	{ "printBytecode", 0, native_printBytecode },
	{ "getImportedLibraries", 0, native_getImportedLibraries },
	{ "pow", 2, native_pow },
	{ "ln", 1, native_ln },
	{ "log", 1, native_log },
	{ "getProgramArgs", 0, native_getProgramArgs },
	{ "io_read", 0, native_read },
	{ "io_readRaw", 0, native_readRaw },
	{ "io_readFile", 1, native_readFile },
	{ "io_writeFile", 2, native_writeFile },
	{ "vm_getRefs", 1, native_vm_getRefs },
	{ "vm_getAt", 2, native_vm_getAt },
	{ "process_execute", 1, native_process_execute },
	{ "dispatch", 1, native_dispatch },
	{ "random_float", 0, native_random_float },
	{ 0, 0, 0 }
};

static size_t end_ptr = 0;
void register_native_call(const char* name, size_t num_args,
    struct data (*function)(struct vm*, struct data*)) {

	while (native_functions[end_ptr].name != NULL) {
		end_ptr++;
	}
	native_functions[end_ptr].name = name;
	native_functions[end_ptr].argc = num_args;
	native_functions[end_ptr].function = function;
	end_ptr++;
	native_functions[end_ptr].name = NULL;
}


double native_to_numeric(struct vm* vm, struct data* t) {
	if (t->type != D_NUMBER) {
		error_runtime(vm->memory, vm->line, VM_INVALID_NATIVE_NUMERICAL_TYPE_ERROR);
		return 0;
	}
	return t->value.number;
}

char* native_to_string(struct vm* vm, struct data* t) {
	if (t->type != D_STRING) {
		error_runtime(vm->memory, vm->line, VM_INVALID_NATIVE_STRING_TYPE_ERROR);
		return "";
	}
	return t->value.string;
}

static struct data native_dispatch(struct vm* vm, struct data* args) {
	struct data *fn = &args[0];
	if (fn->type != D_FUNCTION) {
		error_runtime(vm->memory, vm->line, "Expected function to dispatch!");
	}
	return none_data();
}

static struct data native_getProgramArgs(struct vm* vm, struct data* args) {
	UNUSED(args);
	UNUSED(vm);
	struct data* array = wendy_list_malloc(vm->memory, program_arguments_count);
	int size = 1; // 1st is the header
	for (int i = 0; i < program_arguments_count; i++) {
		array[size++] = make_data(D_STRING,
			data_value_str(program_arguments[i]));
	}
	return make_data(D_LIST, data_value_ptr(array));
}

static struct data native_getImportedLibraries(struct vm* vm, struct data* args) {
	UNUSED(args);
	UNUSED(vm);
	struct import_node *node = imported_libraries;
	// Traverse once to find length
	size_t length = 0;
	while (node) {
		length++;
		node = node->next;
	}
	struct data* library_list = wendy_list_malloc(vm->memory, length);
	node = imported_libraries;
	length = 1;
	while (node) {
		library_list[length++] =
			make_data(D_STRING, data_value_str(node->name));
		node = node->next;
	}
	return make_data(D_LIST, data_value_ptr(library_list));
}

static struct data native_getc(struct vm* vm, struct data* args) {
	UNUSED(args);
	UNUSED(vm);
	char result[2];
	result[0] = getc(stdin);
	result[1] = 0;
	return make_data(D_STRING, data_value_str(result));
}

static struct data native_pow(struct vm* vm, struct data* args) {
	double a = native_to_numeric(vm, args);
	double b = native_to_numeric(vm, args + 1);
	return make_data(D_NUMBER, data_value_num(pow(a, b)));
}

static struct data native_ln(struct vm* vm, struct data* args) {
	return make_data(D_NUMBER, data_value_num(log(native_to_numeric(vm, args))));
}

static struct data native_log(struct vm* vm, struct data* args) {
	return make_data(D_NUMBER, data_value_num(log10(native_to_numeric(vm, args))));
}

static struct data native_exec(struct vm* vm, struct data* args) {
	char* command = native_to_string(vm, args);
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		return make_data(D_NUMBER, data_value_num(system(command)));
	}
	return noneret_data();
}

static struct data native_process_execute(struct vm* vm, struct data* args) {
	// args[0] should be a command, args[1] should be list!
	if (get_settings_flag(SETTINGS_SANDBOXED)) {
		struct data * result_list = wendy_list_malloc(vm->memory, 2);
		result_list[1] = make_data(D_NUMBER, data_value_num(0));
		struct data * output_list = wendy_list_malloc(vm->memory, 0);
		result_list[2] = make_data(D_LIST, data_value_ptr(output_list));
		return make_data(D_LIST, data_value_ptr(result_list));
	}
	char* command = native_to_string(vm, args);

	FILE* fd = safe_popen(command, "r");
	if (!fd) {
		error_runtime(vm->memory, vm->line, "Internal error opening file descriptor with popen!");
		return none_data();
	}

	// Start with 256 lines.
	size_t capacity = 256, size = 0;
	struct data* answer_buffer = safe_calloc(capacity, sizeof(struct data));

	char line_buffer[INPUT_BUFFER_SIZE];
	bool last_has_newline = true;
	while (fgets(line_buffer, sizeof(line_buffer), fd)) {
		bool old_has_newline = last_has_newline;
		last_has_newline = false;
		for (size_t i = 0; line_buffer[i]; i++) {
			if (line_buffer[i] == '\n') {
				line_buffer[i] = 0;
				if (old_has_newline) {
					// Make new!
					if (size == capacity) {
						capacity *= 2;
						answer_buffer = safe_realloc(answer_buffer, capacity);
					}
					answer_buffer[size++] = make_data(D_STRING, data_value_str(line_buffer));
				}
				else {
					// Append to the last one
					size_t old = strlen(answer_buffer[size - 1].value.string);
					size_t total = old + strlen(line_buffer);
					answer_buffer[size - 1].value.string = safe_realloc(answer_buffer[size - 1].value.string, total + 1);
					strcat(&answer_buffer[size - 1].value.string[old], line_buffer);
				}
				last_has_newline = true;
				break;
			}
		}
	}

	// This is a refcnted list, we allocate here once we know the size;
	//   we also steal the data over from the answer_buffer;
	struct data * wendy_list = wendy_list_malloc(vm->memory, size);
	for (size_t i = 0; i < size; i++) {
		wendy_list[i + 1] = answer_buffer[i];
	}

	// No need to destroy data when we steal it.
	safe_free(answer_buffer);

	int status = safe_pclose(fd);

	struct data * result_list = wendy_list_malloc(vm->memory, 2);
	result_list[1] = make_data(D_NUMBER, data_value_num(status));
	result_list[2] = make_data(D_LIST, data_value_ptr(wendy_list));
	return make_data(D_LIST, data_value_ptr(result_list));
}

static struct data native_printCallStack(struct vm* vm, struct data* args) {
	print_call_stack(vm->memory, stdout, (int)native_to_numeric(vm, args));
	return noneret_data();
}

static struct data native_printBytecode(struct vm* vm, struct data* args) {
	UNUSED(args);
	print_current_bytecode(vm);
	return noneret_data();
}

static struct data native_reverseString(struct vm* vm, struct data* args) {
	char* string = native_to_string(vm, args);
	int len = strlen(string);
	struct data t = make_data(D_STRING, data_value_str(string));
	for (int i = 0; i < len / 2; i++) {
		char tmp = t.value.string[i];
		t.value.string[i] = t.value.string[len - i - 1];
		t.value.string[len - i - 1] = tmp;
	}
	return t;
}

static struct data native_stringToInteger(struct vm* vm, struct data* args) {
	char* s = native_to_string(vm, args);
	int integer = 0;
	bool neg = false;
	for (int i = 0; s[i]; i++) {
		if (s[i] == '-') {
			neg = !neg;
		}
		else if (s[i] >= '0' && s[i] <= '9') {
			integer *= 10;
			integer += s[i] - '0';
		}
	}
	if (neg) integer *= -1;
	return make_data(D_NUMBER, data_value_num(integer));
}

static struct data native_read(struct vm* vm, struct data* args) {
	UNUSED(args);
	UNUSED(vm);
	// Scan one line from the input.
	char buffer[INPUT_BUFFER_SIZE];
	while(!fgets(buffer, INPUT_BUFFER_SIZE, stdin)) {};

	char* end_ptr = buffer;
	errno = 0;
	double d = strtod(buffer, &end_ptr);
	// null terminator or newline character
	if (errno != 0 || (*end_ptr != 0 && *end_ptr != 10)) {
		size_t len = strlen(buffer);
		// remove last newline
		buffer[len - 1] = 0;
		return make_data(D_STRING, data_value_str(buffer));
	}
	return make_data(D_NUMBER, data_value_num(d));
}

static struct data native_readRaw(struct vm* vm, struct data* args) {
	UNUSED(args);
	UNUSED(vm);
	// Scan one line from the input.
	char buffer[INPUT_BUFFER_SIZE];
	while(!fgets(buffer, INPUT_BUFFER_SIZE, stdin)) {};
	size_t len = strlen(buffer);
	buffer[len - 1] = 0;
	return make_data(D_STRING, data_value_str(buffer));
}

static struct data native_readFile(struct vm* vm, struct data* args) {
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		char* file = native_to_string(vm, args);
		FILE *f = fopen(file, "rb");
		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);  //same as rewind(f);
		union data_value r;
		r.string = safe_malloc(fsize + 1);
		fread(r.string, fsize, 1, f);
		fclose(f);
		r.string[fsize] = 0;
		return make_data(D_STRING, r);
	}
	return noneret_data();
}

static struct data native_writeFile(struct vm* vm, struct data* args) {
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		char* file = native_to_string(vm, args);
		/* Either the content is a string, or it's a array of
		 * integers that represent the bytes */
		struct data content = args[1];
		FILE *f = fopen(file, "wb");
		if (content.type == D_STRING) {
			char* content_string = native_to_string(vm, args + 1);
			fwrite(content_string, 1, strlen(content_string), f);
		}
		else if (content.type == D_LIST) {
			struct data *result = content.value.reference;
			size_t list_size = result[0].value.number;
			for (size_t i = 0; i < list_size; i++) {
				fputc((int)native_to_numeric(vm, result + i + 1), f);
			}
		}
		else {
			error_runtime(vm->memory, vm->line, "Expected string or list to write to file.");
		}
		fclose(f);
	}
	return noneret_data();
}

static struct data native_vm_getRefs(struct vm* vm, struct data* args) {
	struct data arg = args[0];
	if (!is_reference(arg)) {
		error_runtime(vm->memory, vm->line, "Passed argument is not a reference type!");
		return none_data();
	}
	struct refcnt_container* container_info =
		(struct refcnt_container*)(
			(unsigned char*) arg.value.reference - sizeof(struct refcnt_container)
		);

	return make_data(D_NUMBER, data_value_num(container_info->refs));
}

static struct data native_vm_getAt(struct vm* vm, struct data* args) {
	struct data ref = args[0];
	double index = native_to_numeric(vm, &args[1]);
	if (!is_reference(ref)) {
		error_runtime(vm->memory, vm->line, "Passed argument is not a reference type!");
		return none_data();
	}
	struct data result = copy_data(ref.value.reference[(int) index]);
	if (is_numeric(result)) {
		result.type = D_NUMBER;
	}
	else if (is_vm_internal_type(result)) {
		if (!is_reference(result)) {
			result.type = D_STRING;
		}
	}
	return result;
}

void native_call(struct vm* vm, char* function_name, int expected_args) {
	bool found = false;
	for (int i = 0; ; i++) {
		if (native_functions[i].name == NULL) break;
		if(streq(native_functions[i].name, function_name)) {
			int argc = native_functions[i].argc;
			if (expected_args != argc) {
				error_runtime(vm->memory, vm->line, VM_INVALID_NATIVE_NUMBER_OF_ARGS, function_name);
			}
			struct data* arg_list = safe_malloc(sizeof(struct data) * argc);
			for (int j = 0; j < argc; j++) {
				arg_list[j] = pop_arg(vm->memory, vm->line);
			}
			struct data end_marker = pop_arg(vm->memory, vm->line);
			if (end_marker.type != D_END_OF_ARGUMENTS) {
				error_runtime(vm->memory, vm->line, VM_INVALID_NATIVE_NUMBER_OF_ARGS, function_name);
			}
			push_arg(vm->memory, native_functions[i].function(vm, arg_list));
			for (int i = 0; i < argc; i++) {
				destroy_data_runtime(vm->memory, &arg_list[i]);
			}
			safe_free(arg_list);
			found = true;
			break;
		}
	}
	if (!found) {
		error_runtime(vm->memory, vm->line, VM_INVALID_NATIVE_CALL, function_name);
	}
}


static struct data native_random_float(struct vm* vm, struct data* args) {
	UNUSED(args);
	UNUSED(vm);
	return make_data(D_NUMBER, data_value_num(rand() / (double) RAND_MAX));
}