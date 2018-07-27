#include "memory.h"
#include "error.h"
#include "execpath.h"
#include "global.h"
#include "native.h"
#include "scanner.h"
#include "debugger.h"
#include "ast.h"
#include "vm.h"
#include "codegen.h"
#include "source.h"
#include "optimizer.h"
#include "data.h"
#include "dependencies.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
char* readline(char* prompt) {
	fputs(prompt, stdout);
	char* cpy = safe_malloc(INPUT_BUFFER_SIZE);
	fgets(cpy, INPUT_BUFFER_SIZE, stdin);
	return cpy;
}

void add_history(char* prompt) {
	return;
}

void clear_console(void) {
	system("cls");
}
#else
#include <readline/readline.h>
#include <readline/history.h>

void clear_console(void) {
	system("clear");
}
#endif

// WendyScript Interpreter in C
// By: Felix Guo
//
// main.c: used to handle REPL and calling the interpreter on a file.

void invalid_usage(void) {
	printf("usage: wendy [file|string] [options] [arguments to program] \n\n");
	printf("    [file|string]     : is either a compiled WendyScript file, a raw source file, or a source string to run.\n\n");
	printf("Arguments to program can be read using the system module.\n\n");
	printf("Options:\n");
	printf("    -h, --help        : shows this message.\n");
	printf("    --nogc            : disables garbage-collection.\n");
	printf("    --optimize        : enables optimization algorithm (this will destroy overloaded primitive operators).\n");
	printf("    -c, --compile     : compiles the given file but does not run.\n");
	printf("    -v, --verbose     : displays information about memory state on error.\n");
	printf("    --ast             : prints out the constructed AST.\n");
	printf("    -t, --token-list  : prints out the parsed tokens.\n");
	printf("    -d --disassemble  : prints out the disassembled bytecode.\n");
	printf("    --dependencies    : prints out the module dependencies of the file.\n");
	printf("    --sandbox         : runs the VM in sandboxed mode, ie. no file access and no native execution calls.\n");
	printf("\nWendy will enter REPL mode if no parameters are supplied.\n");
	safe_exit(1);
}

// The first non-valid option is typically the file name / source string.
// The other non-valid options are the arguments.
// Returns true if user prompted for help.
bool process_options(char** options, int len, char** source) {
	int i;
	bool has_encountered_invalid = false;
	*source = NULL;
	for (i = 0; i < len; i++) {
		if (streq("-c", options[i]) ||
			streq("--compile", options[i])) {
			set_settings_flag(SETTINGS_COMPILE);
		}
		else if (streq("-v", options[i]) ||
			streq("--verbose", options[i])) {
			set_settings_flag(SETTINGS_VERBOSE);
		}
		else if (streq("--nogc", options[i])) {
			set_settings_flag(SETTINGS_NOGC);
		}
		else if (streq("--optimize", options[i])) {
			set_settings_flag(SETTINGS_OPTIMIZE);
		}
		else if (streq("--ast", options[i])) {
			set_settings_flag(SETTINGS_ASTPRINT);
		}
		else if (streq("--dependencies", options[i])) {
			set_settings_flag(SETTINGS_OUTPUT_DEPENDENCIES);
		}
		else if (streq("--sandbox", options[i])) {
			set_settings_flag(SETTINGS_SANDBOXED);
		}
		else if (streq("-t", options[i]) ||
				 streq("--token-list", options[i])) {
			set_settings_flag(SETTINGS_TOKEN_LIST_PRINT);
		}
		else if (streq("-d", options[i]) ||
				 streq("--disassemble", options[i])) {
			set_settings_flag(SETTINGS_DISASSEMBLE);
		}
		else if (streq("-d", options[i]) ||
				 streq("--disassemble", options[i])) {
			set_settings_flag(SETTINGS_DISASSEMBLE);
		}
		else if (streq("-h", options[i]) ||
				 streq("--help", options[i])) {
			return true;
		}
		else if (!has_encountered_invalid) {
			*source = options[i];
			has_encountered_invalid = true;
		}
		else {
			// Once we hit a not-valid option, the rest are all arguments to
			//   program.
			break;
		}
	}
	program_arguments_count = len - i;
	program_arguments = &options[i];
	return false;
}

void run(char* input_string) {
	size_t alloc_size = 0;
	token* tokens;
	size_t tokens_count;
	tokens_count = scan_tokens(input_string, &tokens, &alloc_size);
	statement_list* ast = generate_ast(tokens, tokens_count);
	if (get_settings_flag(SETTINGS_OPTIMIZE)) {
		ast = optimize_ast(ast);
	}
	if(!ast_error_flag()) {
		size_t size;
		uint8_t* bytecode = generate_code(ast, &size);
		if (get_settings_flag(SETTINGS_DISASSEMBLE)) {
			print_bytecode(bytecode, stdout);
		}
		vm_run(bytecode, size);
		safe_free(bytecode);
	}
	free_ast(ast);
	safe_free(tokens);
}

bool bracket_check(char* source) {
	if (!source[0]) return false;
	// Parentheses, Square, Curly, Single Quote, Double Quote
	int p = 0, s = 0, c = 0;
	char in_string = 0;
	for (int i = 0; source[i]; i++) {
		if (in_string && source[i] == '\\') {
			i++;
			continue;
		}
		switch(source[i]) {
			case '(': p++; break;
			case ')': p--; break;
			case '[': s++; break;
			case ']': s--; break;
			case '{': c++; break;
			case '}': c--; break;
			case '"': {
				in_string = in_string == '"' ? 0 : '"';
				break;
			}
			case '\'': {
				in_string = in_string == '\'' ? 0 : '\'';
				break;
			}
		}
	}
	return !p && !s && !c && !in_string;
}

int repl(void) {
	init_source(0, "", 0, false);
	printf("Welcome to " WENDY_VERSION " created by: Felix Guo\n");
	printf(BUILD_VERSION "\n");
	printf("Run `wendy -help` to get help.\n");
	printf("Press Ctrl+D (EOF) to exit REPL.\n");
	char* path = get_path();
	printf("Path: %s\n", path);
	printf("Note: REPL does not show call stack on error, run with -v to dump call stacks.\n");
	safe_free(path);
	char* input_buffer;
	char* source_to_run = safe_malloc(1 * sizeof(char));
	// ENTER REPL MODE
	set_settings_flag(SETTINGS_REPL);
	push_frame("main", 0, 0);
	bool has_run = false;
	forever {
		size_t source_size = 1;
		source_to_run[0] = 0;
		bool first = true;
		// Perform bracket check to determine whether or not to execute:
		while (!bracket_check(source_to_run)) {
			if (first) {
				input_buffer = readline("> ");
				first = false;
			}
			else {
				input_buffer = readline("  ");
			}
			if(!input_buffer || streq(input_buffer, "quit")) {
				printf("\n");
				goto cleanup;
			}
			source_size += strlen(input_buffer);
			source_to_run = safe_realloc(source_to_run,
				source_size * sizeof(char));
			strcat(source_to_run, input_buffer);
			free(input_buffer);
		}
		add_history(source_to_run);
		run(source_to_run);
		has_run = true;
		unwind_stack();
	}
	safe_free(source_to_run);

cleanup:
	c_free_memory();
	if (has_run) {
		vm_cleanup_if_repl();
	}
	return 0;
}

int main(int argc, char** argv) {
	init_memory();
	determine_endianness();
	char *option_result;
	if (process_options(&argv[1], argc - 1, &option_result)) {
		// User asked for -h / --help
		invalid_usage();
	}
	if (!option_result) {
		return repl();
	}
	set_settings_flag(SETTINGS_STRICT_ERROR);
	// FILE READ MODE
	long length = 0;
	int file_name_length = strlen(option_result);
	FILE* file = fopen(option_result, "r");
	if (!file) {
		// Attempt to run as source string
		push_frame("main", 0, 0);
		run(option_result);
		goto wendy_exit;
	}
	// Compute File Size
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek (file, 0, SEEK_SET);
	bool is_compiled = false;
	int header_length = strlen(WENDY_VM_HEADER);
	if (length > header_length) {
		char header[header_length + 1];
		fread(header, sizeof(char), header_length + 1, file);
		fseek(file, 0, SEEK_SET);
		if (streq(WENDY_VM_HEADER, header)) {
			is_compiled = true;
		}
	}
	// File Pointer should be reset
	uint8_t* bytecode_stream;
	size_t size;
	if (!is_compiled) {
		init_source(file, option_result, length, true);
		// Text Source
		char* buffer = get_source_buffer();
		// Begin Processing the File
		size_t alloc_size = 0;
		token* tokens;
		size_t tokens_count;

		// Scanning and Tokenizing
		tokens_count = scan_tokens(buffer, &tokens, &alloc_size);
		if (get_settings_flag(SETTINGS_TOKEN_LIST_PRINT)) {
			print_token_list(tokens, tokens_count);
		}

		// Build AST
		statement_list* ast = generate_ast(tokens, tokens_count);
		if (get_settings_flag(SETTINGS_OPTIMIZE)) {
			ast = optimize_ast(ast);
		}
		if (get_settings_flag(SETTINGS_ASTPRINT)) {
			print_ast(ast);
		}

		if (get_settings_flag(SETTINGS_OUTPUT_DEPENDENCIES)) {
			// Perform static analysis to output dependencies
			print_dependencies(ast);
			safe_free(tokens);
			free_ast(ast);
			goto wendy_exit;
		}
		else {
			// Generate Bytecode
			bytecode_stream = generate_code(ast, &size);
		}
		safe_free(tokens);
		free_ast(ast);
	}
	else {
		// Compiled Source
		char* search_name = safe_malloc(sizeof(char) *
				(strlen(option_result) + 1));
		strcpy(search_name, option_result);
		int i = strlen(search_name);
		while (search_name[i] != '.') i--;
		search_name[i + 1] = 0;
		strcat(search_name, "w");
		FILE* source_file = fopen(search_name, "r");
		if (source_file) {
			fseek(source_file, 0, SEEK_END);
			long length = ftell(source_file);
			fseek (source_file, 0, SEEK_SET);
			init_source(source_file, search_name, length, false);
			fclose(source_file);
		}
		else {
			init_source(0, "", 0, false);
		}
		safe_free(search_name);
		bytecode_stream = safe_malloc(sizeof(uint8_t) * length);
		fread(bytecode_stream, sizeof(uint8_t), length, file);
	}
	fclose(file);
	if (get_settings_flag(SETTINGS_DISASSEMBLE)) {
		print_bytecode(bytecode_stream, stdout);
	}
	if (get_settings_flag(SETTINGS_COMPILE)) {
		if (is_compiled) {
			printf("Compile flag set, but input file was already compiled.\n");
		}
		else {
			int new_file_length = file_name_length + 2;
			char* compile_path = safe_malloc(new_file_length * sizeof(char));
			compile_path[0] = 0;
			strcpy(compile_path, option_result);
			strcat(compile_path, "c");

			FILE* compile_file = fopen(compile_path, "w");
			if (compile_file) {
				write_bytecode(bytecode_stream, compile_file);
				printf("Successfully compiled into %s.\n", compile_path);
			}
			else {
				printf("Error opening compile file to write.\n");
			}
			if (compile_path) safe_free(compile_path);
		}
	}
	else {
		push_frame("main", 0, 0);
		vm_run(bytecode_stream, size);
		if (!last_printed_newline) {
			printf("\n");
		}
	}
	safe_free(bytecode_stream);

wendy_exit:
	free_source();
	c_free_memory();
	check_leak();
	return 0;
}
