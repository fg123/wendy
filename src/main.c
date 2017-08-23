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
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
char* readline(char* prompt) {
    fputs(prompt, stdout);
    char* cpy = safe_malloc(OP_INPUT_BUFFER_SIZE);
    fgets(cpy, OP_INPUT_BUFFER_SIZE, stdin);
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
    printf("usage: wendy [file] [-help] [-nogc] [-c] [-ast] [-disassemble] \n\n");
    printf("    file            : is either a compiled WendyScript file, or a raw source file.\n");
    printf("    -h, --help      : shows this message.\n");
    printf("    --nogc          : disables garbage-collection.\n");
    printf("    --noop          : disables optimization algorithm.\n");
    printf("    -c, --compile   : compiles the given file but does not run.\n");
    printf("    --ast           : prints out the constructed AST.\n");
    printf("    --toklst        : prints out the parsed tokens.\n");
    printf("    --disassemble   : prints out the disassembled bytecode.\n");
    printf("\nWendy will enter REPL mode if no parameters are supplied.\n");
//  printf("\n-d file b1 b2 ...\n   Enables debugging mode, with an output file and an initial set of breakpoints.");

    safe_exit(1);
}

void process_options(char** options, int len) {
    for (int i = 0; i < len; i++) {
        if (strcmp("-c", options[i]) == 0 ||  
            strcmp("--compile", options[i]) == 0) {
            set_settings_flag(SETTINGS_COMPILE);
        }
        else if (strcmp("--nogc", options[i]) == 0) {
            set_settings_flag(SETTINGS_NOGC);
        }
        else if (strcmp("--noop", options[i]) == 0) {
            set_settings_flag(SETTINGS_NOOP);
        }
        else if (strcmp("--ast", options[i]) == 0) {
            set_settings_flag(SETTINGS_ASTPRINT);
        }
        else if (strcmp("--toklst", options[i]) == 0) {
            set_settings_flag(SETTINGS_TOKEN_LIST_PRINT);
        }
        else if (strcmp("--disassemble", options[i]) == 0) {
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
    if (!get_settings_flag(SETTINGS_NOOP)) {
        ast = optimize_ast(ast);
    }
    //print_ast(ast);   
    // Generate Bytecode
    //printf("%d\n", ast_error_flag());
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
    int p = 0, s = 0, c = 0, sq = 0, dq = 0;
    for (int i = 0; source[i]; i++) {
        switch(source[i]) {
            case '(': p++; break;
            case ')': p--; break;
            case '[': s++; break;
            case ']': s--; break;
            case '{': c++; break;
            case '}': c--; break;
            case '"': dq++; break;
            case '\'': sq++; break;
        }
    }
    return !p && !s && !c && !(dq % 2) && !(sq % 2);
}

int main(int argc, char** argv) {
    init_memory();
    determine_endianness();
    if (argc == 1) {
        init_source(0, "", 0, false);
        clear_console();
        printf("Welcome to %s created by: Felix Guo\n", WENDY_VERSION);
        printf("Run `wendy -help` to get help. \n");
        printf("Press Ctrl+D (EOF) to exit REPL.\n");
        char* path = get_path();
        printf("Path: %s\n", path);
        safe_free(path);
        char* input_buffer;
        char* source_to_run = safe_malloc(1 * sizeof(char));
        // ENTER REPL MODE
        set_settings_flag(SETTINGS_NOOP);
        set_settings_flag(SETTINGS_REPL);        
        push_frame("main", 0, 0);

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
                    c_free_memory();
                    vm_cleanup_if_repl();
                    return 0;
                }
                source_size += strlen(input_buffer);
                source_to_run = safe_realloc(source_to_run, 
                    source_size * sizeof(char));
                strcat(source_to_run, input_buffer);
                free(input_buffer);
            }
            add_history(source_to_run);
            run(source_to_run);
        }
        safe_free(source_to_run);
        c_free_memory();
        vm_cleanup_if_repl();
        return 0;
    }
    set_settings_flag(SETTINGS_STRICT_ERROR);
    if (argc != 2) {
        process_options(&argv[2], argc - 2);
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        invalid_usage();
    }
    else if (argc >= 2) {
        // FILE OP_READ MODE
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
            if (!get_settings_flag(SETTINGS_NOOP)) {
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
