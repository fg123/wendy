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
typedef struct native_function {
	char* name;
	int argc;
	data (*function)(data*, int);
} native_function;

static data native_printCallStack(data* args, int line);
static data native_reverseString(data* args, int line);
static data native_stringToInteger(data* args, int line);
static data native_exec(data* args, int line);
static data native_getc(data* args, int line);
static data native_printBytecode(data* args, int line);
static data native_getImportedLibraries(data* args, int line);
static data native_getProgramArgs(data* args, int line);
static data native_read(data* args, int line);
static data native_readRaw(data* args, int line);
static data native_readFile(data* args, int line);
static data native_writeFile(data* args, int line);

// Math Functions
static data native_pow(data* args, int line);
static data native_ln(data* args, int line);
static data native_log(data* args, int line);

static native_function native_functions[] = {
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
	{ "io_writeFile", 2, native_writeFile }
};

static double native_to_numeric(data* t, int line) {
	UNUSED(line);
	if (t->type != D_NUMBER) {
		error_runtime(line, VM_INVALID_NATIVE_NUMERICAL_TYPE_ERROR);
		return 0;
	}
	return t->value.number;
}

static char* native_to_string(data* t, int line) {
	UNUSED(line);
	if (t->type != D_STRING) {
		error_runtime(line, VM_INVALID_NATIVE_STRING_TYPE_ERROR);
		return "";
	}
	return t->value.string;
}

static data native_getProgramArgs(data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	data* array = wendy_list_malloc(program_arguments_count);
	int size = 1; // 1st is the header
	for (int i = 0; i < program_arguments_count; i++) {
		array[size++] = make_data(D_STRING,
			data_value_str(program_arguments[i]));
	}
	return make_data(D_LIST, data_value_ptr(array));
}

static data native_getImportedLibraries(data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	import_node *node = imported_libraries;
	// Traverse once to find length
	size_t length = 0;
	while (node) {
		length++;
		node = node->next;
	}
	data* library_list = wendy_list_malloc(length);
	node = imported_libraries;
	length = 1;
	while (node) {
		library_list[length++] =
			make_data(D_STRING, data_value_str(node->name));
		node = node->next;
	}
	return make_data(D_LIST, data_value_ptr(library_list));
}

static data native_getc(data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	char result[2];
	result[0] = getc(stdin);
	result[1] = 0;
	return make_data(D_STRING, data_value_str(result));
}

static data native_pow(data* args, int line) {
	double a = native_to_numeric(args, line);
	double b = native_to_numeric(args + 1, line);
	return make_data(D_NUMBER, data_value_num(pow(a, b)));
}

static data native_ln(data* args, int line) {
	return make_data(D_NUMBER, data_value_num(log(native_to_numeric(args, line))));
}

static data native_log(data* args, int line) {
	return make_data(D_NUMBER, data_value_num(log10(native_to_numeric(args, line))));
}

static data native_exec(data* args, int line) {
	char* command = native_to_string(args, line);
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		return make_data(D_NUMBER, data_value_num(system(command)));
	}
	return noneret_data();
}

static data native_printCallStack(data* args, int line) {
	print_call_stack(stdout, (int)native_to_numeric(args, line));
	return noneret_data();
}

static data native_printBytecode(data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	print_current_bytecode();
	return noneret_data();
}

static data native_reverseString(data* args, int line) {
	char* string = native_to_string(args, line);
	int len = strlen(string);
	data t = make_data(D_STRING, data_value_str(string));
	for (int i = 0; i < len / 2; i++) {
		char tmp = t.value.string[i];
		t.value.string[i] = t.value.string[len - i - 1];
		t.value.string[len - i - 1] = tmp;
	}
	return t;
}

static data native_stringToInteger(data* args, int line) {
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

static data native_read(data* args, int line) {
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

static data native_readRaw(data* args, int line) {
	UNUSED(args);
	UNUSED(line);
	// Scan one line from the input.
	char buffer[INPUT_BUFFER_SIZE];
	while(!fgets(buffer, INPUT_BUFFER_SIZE, stdin)) {};
	size_t len = strlen(buffer);
	buffer[len - 1] = 0;
	return make_data(D_STRING, data_value_str(buffer));
}

static data native_readFile(data* args, int line) {
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		char* file = native_to_string(args, line);
		FILE *f = fopen(file, "rb");
		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);  //same as rewind(f);
		data_value r;
		r.string = safe_malloc(fsize + 1);
		fread(r.string, fsize, 1, f);
		fclose(f);
		r.string[fsize] = 0;
		return make_data(D_STRING, r);
	}
	return noneret_data();
}

static data native_writeFile(data* args, int line) {
	if (!get_settings_flag(SETTINGS_SANDBOXED)) {
		char* file = native_to_string(args, line);
		char* content = native_to_string(args + 1, line);
		FILE *f = fopen(file, "wb");
		fwrite(content, 1, strlen(content), f);
	}
	return noneret_data();
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
			data* arg_list = safe_malloc(sizeof(data) * argc);
			for (int j = 0; j < argc; j++) {
				arg_list[j] = pop_arg(line);
			}
			data end_marker = pop_arg(line);
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
