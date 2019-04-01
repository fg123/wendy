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

char** program_arguments = 0;
int program_arguments_count = 0;
struct native_function {
	char* name;
	int argc;
	struct data (*function)(struct data*, int);
};

static struct data native_printCallStack(struct data* args, int line);
static struct data native_reverseString(struct data* args, int line);
static struct data native_stringToInteger(struct data* args, int line);
static struct data native_exec(struct data* args, int line);
static struct data native_getc(struct data* args, int line);
static struct data native_printBytecode(struct data* args, int line);
static struct data native_getImportedLibraries(struct data* args, int line);
static struct data native_getProgramArgs(struct data* args, int line);
static struct data native_read(struct data* args, int line);
static struct data native_readRaw(struct data* args, int line);
static struct data native_readFile(struct data* args, int line);
static struct data native_writeFile(struct data* args, int line);

static struct data native_vm_getRefs(struct data* args, int line);

// Math Functions
static struct data native_pow(struct data* args, int line);
static struct data native_ln(struct data* args, int line);
static struct data native_log(struct data* args, int line);

static struct native_function native_functions[] = {
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
	{ "vm_getRefs", 1, native_vm_getRefs }
};

static double native_to_numeric(struct data* t, int line) {
	UNUSED(line);
	if (t->type != D_NUMBER) {
		error_runtime(line, VM_INVALID_NATIVE_NUMERICAL_TYPE_ERROR);
		return 0;
	}
	return t->value.number;
}

static char* native_to_string(struct data* t, int line) {
	UNUSED(line);
	if (t->type != D_STRING) {
		error_runtime(line, VM_INVALID_NATIVE_STRING_TYPE_ERROR);
		return "";
	}
	return t->value.string;
}

static struct data native_getProgramArgs(struct data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	struct data* array = wendy_list_malloc(program_arguments_count);
	int size = 1; // 1st is the header
	for (int i = 0; i < program_arguments_count; i++) {
		array[size++] = make_data(D_STRING,
			data_value_str(program_arguments[i]));
	}
	return make_data(D_LIST, data_value_ptr(array));
}

static struct data native_getImportedLibraries(struct data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	struct import_node *node = imported_libraries;
	// Traverse once to find length
	size_t length = 0;
	while (node) {
		length++;
		node = node->next;
	}
	struct data* library_list = wendy_list_malloc(length);
	node = imported_libraries;
	length = 1;
	while (node) {
		library_list[length++] =
			make_data(D_STRING, data_value_str(node->name));
		node = node->next;
	}
	return make_data(D_LIST, data_value_ptr(library_list));
}

static struct data native_getc(struct data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	char result[2];
	result[0] = getc(stdin);
	result[1] = 0;
	return make_data(D_STRING, data_value_str(result));
}

static struct data native_pow(struct data* args, int line) {
	double a = native_to_numeric(args, line);
	double b = native_to_numeric(args + 1, line);
	return make_data(D_NUMBER, data_value_num(pow(a, b)));
}

static struct data native_ln(struct data* args, int line) {
	return make_data(D_NUMBER, data_value_num(log(native_to_numeric(args, line))));
}

static struct data native_log(struct data* args, int line) {
	return make_data(D_NUMBER, data_value_num(log10(native_to_numeric(args, line))));
}

static struct data native_exec(struct data* args, int line) {
	char* command = native_to_string(args, line);
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		return make_data(D_NUMBER, data_value_num(system(command)));
	}
	return noneret_data();
}

static struct data native_printCallStack(struct data* args, int line) {
	print_call_stack(stdout, (int)native_to_numeric(args, line));
	return noneret_data();
}

static struct data native_printBytecode(struct data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	print_current_bytecode();
	return noneret_data();
}

static struct data native_reverseString(struct data* args, int line) {
	char* string = native_to_string(args, line);
	int len = strlen(string);
	struct data t = make_data(D_STRING, data_value_str(string));
	for (int i = 0; i < len / 2; i++) {
		char tmp = t.value.string[i];
		t.value.string[i] = t.value.string[len - i - 1];
		t.value.string[len - i - 1] = tmp;
	}
	return t;
}

static struct data native_stringToInteger(struct data* args, int line) {
	char* s = native_to_string(args, line);
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

static struct data native_read(struct data* args, int line) {
	UNUSED(args);
	UNUSED(line);
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

static struct data native_readRaw(struct data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	// Scan one line from the input.
	char buffer[INPUT_BUFFER_SIZE];
	while(!fgets(buffer, INPUT_BUFFER_SIZE, stdin)) {};
	size_t len = strlen(buffer);
	buffer[len - 1] = 0;
	return make_data(D_STRING, data_value_str(buffer));
}

static struct data native_readFile(struct data* args, int line) {
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		char* file = native_to_string(args, line);
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

static struct data native_writeFile(struct data* args, int line) {
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		char* file = native_to_string(args, line);
		/* Either the content is a string, or it's a array of
		 * integers that represent the bytes */
		struct data content = args[1];
		FILE *f = fopen(file, "wb");
		if (content.type == D_STRING) {
			char* content_string = native_to_string(args + 1, line);
			fwrite(content_string, 1, strlen(content_string), f);
		}
		else if (content.type == D_LIST) {
			struct data *result = content.value.reference;
			size_t list_size = result[0].value.number;
			for (size_t i = 0; i < list_size; i++) {
				fputc((int)native_to_numeric(result + i + 1, line), f);
			}
		}
		else {
			error_runtime(line, "Expected string or list to write to file.");
		}
		fclose(f);
	}
	return noneret_data();
}

static struct data native_vm_getRefs(struct data* args, int line) {
	struct data arg = args[0];
	if (!is_reference(arg)) {
		error_runtime(line, "Passed argument is not a reference type!");
		return none_data();
	}
	struct refcnt_container* container_info =
		(void*) arg.value.reference - sizeof(struct refcnt_container);

	return make_data(D_NUMBER, data_value_num(container_info->refs));
}

void native_call(char* function_name, int expected_args, int line) {
	int functions = sizeof(native_functions) / sizeof(native_functions[0]);
	bool found = false;
	for (int i = 0; i < functions; i++) {
		if(streq(native_functions[i].name, function_name)) {
			int argc = native_functions[i].argc;
			if (expected_args != argc) {
				error_runtime(line, VM_INVALID_NATIVE_NUMBER_OF_ARGS, function_name);
			}
			struct data* arg_list = safe_malloc(sizeof(struct data) * argc);
			for (int j = 0; j < argc; j++) {
				arg_list[j] = pop_arg(line);
			}
			struct data end_marker = pop_arg(line);
			if (end_marker.type != D_END_OF_ARGUMENTS) {
				error_runtime(line, VM_INVALID_NATIVE_NUMBER_OF_ARGS, function_name);
			}
			push_arg(native_functions[i].function(arg_list, line));
			for (int i = 0; i < argc; i++) {
				destroy_data(&arg_list[i]);
			}
			safe_free(arg_list);
			found = true;
			break;
		}
	}
	if (!found) {
		error_runtime(line, VM_INVALID_NATIVE_CALL, function_name);
	}
}
