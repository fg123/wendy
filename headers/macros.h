#ifndef MACROS_H
#define MACROS_H

// This file contains information that the majority of WendyScript modules use.

// Main Information
#define WENDY_VERSION "Wendy 1.1"
#define INPUT_BUFFER_SIZE 2048

// Data/Token Information
#define MAX_STRING_LEN 1024
#define MAX_LIST_INIT_LEN 100
#define MAX_STRUCT_META_LEN 100

// CodeGenerator Information

// Always have 1024 more bytes than we need in the allocation.
#define CODEGEN_PAD_SIZE 512
#define CODEGEN_START_SIZE 1024

// Stack Entry Sizes
#define MAX_IDENTIFIER_LEN 59


// VM Memory Limits
#define MEMORY_SIZE 129061
#define STACK_SIZE 100000
#define ARGSTACK_SIZE 128 
#define RESERVED_MEMORY 2
#define CLOSURES_SIZE 128

//typedef unsigned int byte;

#endif
