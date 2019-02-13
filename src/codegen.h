#ifndef CODEGEN_H
#define CODEGEN_H

#include "memory.h"
#include <stdint.h>

// codegen.h - Felix Guo
// Generates the bytecode that runs on the WendyVM, based on the
//   abstract syntax tree that is passed in from the [AST] module.

// WendyVM ByteCode supports operations on Tokens, which is a unit of memory
//   represented by the same type as token. Each unit of memory has an address,
//   which is an unsigned integer.
// WendyVM runs on a stack based operation system and provides operations in the
//   following table.
// Each bytecode file begins with a header before the first bytecode
//   instruction. The header:
// =============================================================================
// WendyVM Bytecode (17 bytes, null terminated)
// =============================================================================
// https://docs.felixguo.me/architecture/wendy/slim-vm.md

typedef enum opcode {
	OP_PUSH,
	OP_BIN,
	OP_UNA,
	OP_CALL,
	OP_RET,
	OP_DECL,
	OP_WRITE,
	OP_OUT,
	OP_OUTL,
	OP_JMP,
	OP_JIF,
	OP_FRM,
	OP_END,
	OP_SRC,
	OP_NATIVE,
	OP_IMPORT,
	OP_ARGCLN,
	OP_MKREF,
	OP_WHERE,
	OP_NTHPTR,
	OP_MEMPTR
} opcode;

#define OPCODE_STRING \
	"push", "bin", "una", "call", "ret", "decl", "write",\
	"out", "outl", "jmp", "jif", "frm", "end", "src",\
	"native", "import", "argcln", "mkref", "where", "nthptr", "memptr"

extern const char* opcode_string[];

// generate_code(ast) generates Wendy ByteCode based on the ast and
//   returns the ByteArray
// effects: allocates memory, caller must free
uint8_t* generate_code(statement_list* ast, size_t* size);

// print_bytecode(bytecode, buffer) prints the given bytecode into
//   a readable format into the buffer.
void print_bytecode(uint8_t* bytecode, FILE* buffer);

// offset_addresses(buffer, length, offset) offsets all instructions
//   that require an address by the given offset
void offset_addresses(uint8_t* buffer, size_t length, int offset);

// write_bytecode(bytecode, buffer) writes the bytecode into binary file
void write_bytecode(uint8_t* bytecode, FILE* buffer);

// get_token(bytecode, end) gets a token from the bytecode stream
data get_data(uint8_t* bytecode, unsigned int* end);

// get_address(bytecode) gets an address from bytecode stream, decoding
//   endianness as required
address get_address(uint8_t* bytecode, unsigned int* end);

// get_string(bytecode, end) returns a pointer to the string as well as
//   advances end to the byte after the string
char *get_string(uint8_t *bytecode, unsigned int *end);

// verify_header(bytecode) checks the header for information,
//   then returns the index of the first opcode instruction
int verify_header(uint8_t* bytecode);

#endif
