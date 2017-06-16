#include <stdio.h>
#include "memory.h"
#include "error.h"
#include "execpath.h"
#include "preprocessor.h"
#include "tests.h"
#include "global.h"
#include "scanner.h"
#include <string.h>
#include "debugger.h"
#include "ast.h"
#include "vm.h"
#include "codegen.h"
#include "source.h"

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

void clear_console() {
	system("cls");
}
#else
#include <readline/readline.h>
#include <readline/history.h>

void clear_console() {
	system("clear");
}
#endif

// WendyScript Interpreter in C
// By: Felix Guo
// 
// main.c: used to handle REPL and calling the interpreter on a file.

void invalid_usage() {
	printf("usage: wendy [file] [-help] [-nogc] [-c] [-ast] [-disassemble] "/*[-d file b1 b2 ...]*/"\n\n");
	printf("    file         : is either a compiled WendyScript file, or a raw source file.\n");
	printf("    -help        : shows this message.\n");
	printf("    -no-gc       : disables garbage-collection.\n");
	printf("    -c           : compiles the given file but does not run.\n");
	printf("    -ast         : prints out the constructed AST.\n");
	printf("    -disassemble : prints out the disassembled bytecode.\n");
	printf("\nWendy will enter REPL mode if no parameters are supplied.\n");
//	printf("\n-d file b1 b2 ...\n	Enables debugging mode, with an output file and an initial set of breakpoints.");

	safe_exit(1);
}

void process_options(char** options, int len) {
	for (int i = 0; i < len; i++) {
		if (strcmp("-c", options[i]) == 0) {
			set_settings_flag(SETTINGS_COMPILE);
		}
		/*else if (strcmp("-d", options[i]) == 0) {
			if (i == len - 1) {
				invalid_usage();
			}
			debug_output_path = options[i + 1];
			i++;
			for (; i < len; i++) {
				add_breakpoint(atoi(options[i]));
			}
		}*/	
		else if (strcmp("-nogc", options[i]) == 0) {
			set_settings_flag(SETTINGS_NOGC);
		}
		else if (strcmp("-ast", options[i]) == 0) {
			set_settings_flag(SETTINGS_ASTPRINT);
		}
		else if (strcmp("-disassemble", options[i]) == 0) {
			set_settings_flag(SETTINGS_DISASSEMBLE);
		}
		else {
			invalid_usage();
		}
	}
}

void run(char* input_string) {
	size_t alloc_size = 0;
	token* tokens;
	size_t tokens_count;

	// Scanning and Tokenizing
	tokens_count = scan_tokens(input_string, &tokens, &alloc_size);

	// Build AST
	statement_list* ast = generate_ast(tokens, tokens_count);
	//print_ast(ast);	
	// Generate Bytecode
	//printf("%d\n", ast_error_flag());
	if(!ast_error_flag()) { 
		uint8_t* bytecode = generate_code(ast);
		vm_run(bytecode);
		safe_free(bytecode);
	}
	free_ast(ast);
	safe_free(tokens);
}

int main(int argc, char** argv) {
	init_memory();
	if (argc == 1) {
		init_source(0, "", 0, false);
		clear_console();
		printf("Welcome to %s created by: Felix Guo\n", WENDY_VERSION);
		printf("Run `wendy -help` to get help. \nPress Ctrl+D (EOF) to exit REPL.\n");
		char* path = get_path();
		printf("Path: %s\n", path);
		safe_free(path);
		char *input_buffer;
		// ENTER REPL MODE
		push_frame("main", 0, 0);
		while (1) {
			input_buffer = readline("> ");
			if(!input_buffer) {
				printf("\n");
				c_free_memory();
				return 0;
			}
			add_history(input_buffer);
			run(input_buffer);
			safe_free(input_buffer);
		}
		c_free_memory();
		return 0;
	}
	set_settings_flag(SETTINGS_STRICT_ERROR);
	if (argc != 2) {
		process_options(&argv[2], argc - 2);
	}
	if (argc == 2 && strcmp(argv[1], "-help") == 0) {
		invalid_usage();
	}
	else if (argc >= 2) {
		// FILE READ MODE
		long length = 0;
		int file_name_length = strlen(argv[1]);
		FILE *file = fopen(argv[1], "r");
		if (!file) {
			printf("Error opening file to determine type.\n");
			safe_exit(1);
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
			if (strcmp(WENDY_VM_HEADER, header) == 0) {
				is_compiled = true;
			}
		}
		// File Pointer should be reset
		uint8_t* bytecode_stream;
		if (!is_compiled) {
			init_source(file, argv[1], length, true);
			// Text Source
			char* buffer = get_source_buffer(); 
			// Begin Processing the File
			size_t alloc_size = 0;
			token* tokens;
			size_t tokens_count;

			// Scanning and Tokenizing
			tokens_count = scan_tokens(buffer, &tokens, &alloc_size);

			// Build AST
			statement_list* ast = generate_ast(tokens, tokens_count);
			if (get_settings_flag(SETTINGS_ASTPRINT)) {
				print_ast(ast);
			}
			
			// Generate Bytecode
			bytecode_stream = generate_code(ast);
			safe_free(tokens);
			free_ast(ast);
		}
		else {
			// Compiled Source
			char* search_name = safe_malloc(sizeof(char) * 
					(strlen(argv[1]) + 1));
			strcpy(search_name, argv[1]);
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
		// bytecode_stream now has the stream of bytecode
		if (get_settings_flag(SETTINGS_DISASSEMBLE)) {
			print_bytecode(bytecode_stream, stdout);
		}
		if (get_settings_flag(SETTINGS_COMPILE)) {
			if (is_compiled) {
				printf("Compile flag set, but input file was already compiled.\n");
			}
			else {
				int new_file_length = file_name_length + 2;
				char* compile_path = safe_malloc(length * sizeof(char));
				strcpy(compile_path, argv[1]);
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
			vm_run(bytecode_stream);
			if (!last_printed_newline) {
				printf("\n");
			}
		}
		safe_free(bytecode_stream);	
	}
	free_source();
	c_free_memory();
	check_leak();
	return 0;
}
