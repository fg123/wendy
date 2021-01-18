#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// global.h - Felix Guo
// This file contains global information and utility functions.

#define ENUM(x) x,
#define STRING(x) #x,

// Main Information
#define WENDY_VERSION "Wendy 2.0"

#ifdef RELEASE
#define BUILD_VERSION "Release (" __DATE__ " " __TIME__ ")"
#else
#define BUILD_VERSION "Debug (" __DATE__ " " __TIME__ ")"
#endif

#define INPUT_BUFFER_SIZE 1024
#define WENDY_VM_HEADER "WendyVM Bytecode"

// Data/Token Information
#define MAX_LIST_INIT_LEN 100
#define MAX_STRUCT_META_LEN 100

#define CODEGEN_PAD_SIZE 256
#define CODEGEN_START_SIZE 1024

// Compiler/VM Settings
#define OPERATOR_OVERLOAD_PREFIX "#@"
#define LOOP_COUNTER_PREFIX ":"

#define DIVIDER "===================="

enum settings_flags {
	SETTINGS_COMPILE = 0,
	SETTINGS_NOGC,
	SETTINGS_ASTPRINT,
	SETTINGS_DISASSEMBLE,
	SETTINGS_STRICT_ERROR,
	SETTINGS_TOKEN_LIST_PRINT,
	SETTINGS_OPTIMIZE,
	SETTINGS_REPL,
	SETTINGS_VERBOSE,
	SETTINGS_OUTPUT_DEPENDENCIES,
	SETTINGS_SANDBOXED,
	SETTINGS_TRACE_VM,
	SETTINGS_TRACE_REFCNT,
    SETTINGS_DRY_RUN,
	SETTINGS_COUNT };

void set_settings_flag(enum settings_flags flag);
bool get_settings_flag(enum settings_flags flag);

struct malloc_node {
	char* filename;
	int line_num;
	size_t size;
	void* ptr;
	struct malloc_node* next;
	struct malloc_node* prev;
};

void determine_endianness(void);
bool streq(const char* a, const char* b);

extern bool is_big_endian;
extern bool last_printed_newline;

char* safe_strdup_impl(const char* s, char* allocated);

/* We safe_malloc in the call so we get the right allocation location */
#define safe_strdup(x) safe_strdup_impl(x, safe_malloc(strlen(x) + 1))
#define UNUSED(var) (void)(var)
#define forever for(;;)

char* safe_concat_impl(char* first, ...);

#define safe_concat(...) safe_concat_impl(__VA_ARGS__, NULL)

// Safe Malloc Implementation
#ifndef RELEASE
#define safe_malloc(size) safe_malloc_impl(size, __FILE__, __LINE__)
#define safe_free(ptr) safe_free_impl(ptr, __FILE__, __LINE__)
#define safe_calloc(num, size) safe_calloc_impl(num, size, __FILE__, __LINE__)
#define safe_realloc(ptr, size) safe_realloc_impl(ptr, size, __FILE__, __LINE__)

#else

// Regular Malloc Implementations
#define safe_malloc(size) safe_release_malloc(size)
#define safe_free(ptr) free(ptr)
#define safe_calloc(num, size) safe_release_calloc(num, size)
#define safe_realloc(ptr, size) safe_release_realloc(ptr, size)

#endif

void* safe_realloc_impl(void* ptr, size_t size, char* filename, int line_num);
void* safe_malloc_impl(size_t size, char* filename, int line_num);
void* safe_calloc_impl(size_t num, size_t size, char* filename, int line_num);
void safe_free_impl(void* ptr, char* filename, int line_num);

void* safe_release_malloc(size_t size);
void* safe_release_calloc(size_t num, size_t size);
void* safe_release_realloc(void* ptr, size_t size);

void free_alloc(void);
void check_leak(void);
void safe_exit(int code);

#ifdef _WIN32
	#define safe_popen _popen
	#define safe_pclose _pclose
#else
	#define safe_popen popen
	#define safe_pclose pclose
#endif
#endif
