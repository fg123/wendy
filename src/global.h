#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdbool.h>
#include <stdlib.h>

// global.h - Felix Guo
// This file contains global information and utility functions.

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
#define MAX_STRING_LEN 1024
#define MAX_LIST_INIT_LEN 100
#define MAX_STRUCT_META_LEN 100

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
#define OPERATOR_OVERLOAD_PREFIX "#@"
#define LOOP_COUNTER_PREFIX ":\")"

#define ENUM(x) x,
#define STRING(x) #x,

#define SETTINGS_COUNT 10
typedef enum {
	SETTINGS_COMPILE = 0,
	SETTINGS_NOGC = 1,
	SETTINGS_ASTPRINT = 2,
	SETTINGS_DISASSEMBLE = 3,
	SETTINGS_STRICT_ERROR = 4,
	SETTINGS_TOKEN_LIST_PRINT = 5,
	SETTINGS_OPTIMIZE = 6,
	SETTINGS_REPL = 7,
	SETTINGS_VERBOSE = 8,
	SETTINGS_OUTPUT_DEPENDENCIES = 9,
	SETTINGS_SANDBOXED = 10 } settings_flags;

void set_settings_flag(settings_flags flag);
bool get_settings_flag(settings_flags flag);

void determine_endianness(void);
bool streq(const char* a, const char* b);

extern bool is_big_endian;
extern bool last_printed_newline;

char* w_strdup(const char* s);
#define strdup(x) w_strdup(x)
#define array_size(foo) (sizeof(foo)/sizeof(foo[0]))
#define UNUSED(var) (void)(var)

// Safe Malloc Implementation
#ifndef RELEASE
#define safe_malloc(size) safe_malloc_impl(size, __FILE__, __LINE__)
#define safe_free(ptr) safe_free_impl(ptr, __FILE__, __LINE__)
#define safe_calloc(num, size) safe_calloc_impl(num, size, __FILE__, __LINE__)
#define safe_realloc(ptr, size) safe_realloc_impl(ptr, size, __FILE__, __LINE__)

#else

// Regular Malloc Implementations
#define safe_malloc(size) malloc(size)
#define safe_free(ptr) free(ptr)
#define safe_calloc(num, size) calloc(num, size)
#define safe_realloc(ptr, size) realloc(ptr, size)

#endif

void* safe_realloc_impl(void* ptr, size_t size, char* filename, int line_num);
void* safe_malloc_impl(size_t size, char* filename, int line_num);
void* safe_calloc_impl(size_t num, size_t size, char* filename, int line_num);
void safe_free_impl(void* ptr, char* filename, int line_num);

void free_alloc(void);
void check_leak(void);
void safe_exit(int code);

#endif
