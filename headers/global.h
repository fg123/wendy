#ifndef GLOBAL_H
#define GLOBAL_H

// global.h - Felix Guo

// This file contains information that the majority of WendyScript modules use.
#include <stdbool.h>
#include <stdlib.h>

// Main Information
#define WENDY_VERSION "Wendy 2.0"
#define INPUT_BUFFER_SIZE 256
#define WENDY_VM_HEADER "WendyVM Bytecode"

// Data/Token Information
#define MAX_STRING_LEN 1024
#define MAX_LIST_INIT_LEN 100
#define MAX_STRUCT_META_LEN 100

// Always have 1024 more bytes than we need in the allocation.
#define CODEGEN_PAD_SIZE 512
#define CODEGEN_START_SIZE 1024

// Stack Entry Sizes
#define MAX_IDENTIFIER_LEN 59

// VM Memory Limits
#define MEMORY_SIZE 129061
#define STACK_SIZE 100000
#define ARGSTACK_SIZE 512 
#define RESERVED_MEMORY 2
#define CLOSURES_SIZE 128
#define MEMREGSTACK_SIZE 100

// Compiler/VM Settings
#define SETTINGS_COUNT 6

typedef enum {
	SETTINGS_COMPILE = 0, 
	SETTINGS_NOGC = 1, 
	SETTINGS_ASTPRINT = 2,
	SETTINGS_DISASSEMBLE = 3,
	SETTINGS_STRICT_ERROR = 4,
	SETTINGS_TOKEN_LIST_PRINT = 5 } settings_flags; 

void set_settings_flag(settings_flags flag);
bool get_settings_flag(settings_flags flag);

// Safe Malloc Implementation
#define safe_malloc(size) safe_malloc_impl(size, __FILE__, __LINE__)
#define safe_free(ptr) safe_free_impl(ptr, __FILE__, __LINE__)
#define safe_calloc(num, size) safe_calloc_impl(num, size, __FILE__, __LINE__)
#define safe_realloc(ptr, size) safe_realloc_impl(ptr, size, __FILE__, __LINE__)

void* safe_realloc_impl(void* ptr, size_t size, char* filename, int line_num);
void* safe_malloc_impl(size_t size, char* filename, int line_num);
void* safe_calloc_impl(size_t num, size_t size, char* filename, int line_num); 
void safe_free_impl(void* ptr, char* filename, int line_num);

void free_alloc();
void check_leak();
void safe_exit(int code);

#endif
