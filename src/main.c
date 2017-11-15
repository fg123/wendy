#include "memory.h"
#include "error.h"
#include "execpath.h"
#include "global.h"
#include "scanner.h"
#include "debugger.h"
#include "ast.h"
#include "vm.h"
#include "codegen.h"
#include "source.h"
#include "optimizer.h"
#include "data.h"
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
    printf("usage: wendy [file|string] [options] \n\n");
    printf("    [file|string]     : is either a compiled WendyScript file, a raw source file, or a source string to run.\n\n");
    printf("Options:\n");
    printf("    -h, --help        : shows this message.\n");
    printf("    --nogc            : disables garbage-collection.\n");
    printf("    --optimize        : enables optimization algorithm (this will destroy overloaded primitive operators).\n");
    printf("    -c, --compile     : compiles the given file but does not run.\n");
    printf("    -v, --verbose     : displays information about memory state on error.\n");
    printf("    --ast             : prints out the constructed AST.\n");
    printf("    -t, --token-list  : prints out the parsed tokens.\n");
    printf("    -d --disassemble  : prints out the disassembled bytecode.\n");
    printf("\nWendy will enter REPL mode if no parameters are supplied.\n");
    safe_exit(1);
}

void process_options(char** options, int len) {
    for (int i = 0; i < len; i++) {
        if (strcmp("-c", options[i]) == 0 ||
            strcmp("--compile", options[i]) == 0) {
            set_settings_flag(SETTINGS_COMPILE);
        }
        else if (strcmp("-v", options[i]) == 0 ||
            strcmp("--verbose", options[i]) == 0) {
            set_settings_flag(SETTINGS_VERBOSE);
        }
        else if (strcmp("--nogc", options[i]) == 0) {
            set_settings_flag(SETTINGS_NOGC);
        }
        else if (strcmp("--optimize", options[i]) == 0) {
            set_settings_flag(SETTINGS_OPTIMIZE);
        }
        else if (strcmp("--ast", options[i]) == 0) {
            set_settings_flag(SETTINGS_ASTPRINT);
        }
        else if (strcmp("-t", options[i]) == 0 ||
                 strcmp("--token-list", options[i]) == 0) {
            set_settings_flag(SETTINGS_TOKEN_LIST_PRINT);
        }
        else if (strcmp("-d", options[i]) == 0 ||
                 strcmp("--disassemble", options[i]) == 0) {
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
    tokens_count = scan_tokens(input_string, &tokens, &alloc_size);
    statement_list* ast = generate_ast(tokens, tokens_count);
    if (get_settings_flag(SETTINGS_OPTIMIZE)) {
        ast = optimize_ast(ast);
    }
    if(!ast_error_flag()) {
        size_t size;
        uint8_t* bytecode = generate_code(ast, &size);
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

int main(int argc, char** argv) {
    init_memory();
    determine_endianness();
    if (argc == 1) {
        init_source(0, "", 0, false);
        clear_console();
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
        while (1) {
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
                if(!input_buffer) {
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
        }
        safe_free(source_to_run);

    cleanup:
        c_free_memory();
        if (has_run) {
            vm_cleanup_if_repl();
        }
        return 0;
    }
    set_settings_flag(SETTINGS_STRICT_ERROR);
    if (argc != 2) {
        process_options(&argv[2], argc - 2);
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "-h") == 0)) {
        invalid_usage();
    }
    else if (argc >= 2) {
        // FILE READ MODE
        long length = 0;
        int file_name_length = strlen(argv[1]);
        FILE* file = fopen(argv[1], "r");
        if (!file) {
            // Attempt to run as source.
            push_frame("main", 0, 0);
            run(argv[1]);
            c_free_memory();
            check_leak();
            return 0;
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
        size_t size;
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

            // Generate Bytecode
            bytecode_stream = generate_code(ast, &size);
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
            vm_run(bytecode_stream, size);
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
