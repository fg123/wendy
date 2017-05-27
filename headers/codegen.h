#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

// codegen.h - Felix Guo
// This module generates the bytecode that runs on the WendyVM, based on the
//   abstract syntax tree that is passed in from the [ast] module.

// WendyVM ByteCode supports operations on Wendos, which is a unit of memory
//   which can be represented with a Token. Each unit of memory has an address,
//   which is an unsigned integer.
// WendyVM runs on a stack based operation system and provides operations in the 
//   following table.
//
// VM Registers: $MR -> Memory Pointer Register
// =============================================================================
// BYTE | OPCODE | PARAMETER | EXPLANATION
// =============================================================================
// 0x00 | PUSH   | [wendo]   | pushes a wendo on the stack
// 0x01 | POP    |           | pops the wendo at the top of the stack into
//                               the address at $MR
// 0x02 | BIN	 | [optoken] | pop two wendos from the stack and performs
//                               a binary operation, then pushes result
// 0x03 | UNA	 | [optoken] | performs a unary operation
// 0x04 | CALL	 |           | top of stack should be address, and moves the 
//                               instruction pointer to the given
//                               address while saving the next instruction
//                               in the stack
// 0x05 | RET	 |           | returns out of the function
// 0x06 | BIND	 | [string]  | binds the identifier to the given memory register
// 0x07 | REQ	 | [size]    | requests memory and stores pointer in memory
//                               register
// 0x08 | WHERE  | [string]  | get memory location of the identifier and stores
//                               in memory register
// 0x09 | OUT    |           | pop and prints token at top of stack
// 0x0A | IN     |           | saves the token at memory register
// 0x0B | PLIST  | [size]    | pops the top n elements into memory locations
//                               in memory register + size, and leaves a ref
// 0x0C | RANGE  |           | creates a range object from top two elements
// 0x0D | PULL   |           | pulls wendo from memory register to stack
// 0x0E | JMP    | [address] | jumps to the given address in bytecode

typedef enum opcode { 
	OPUSH, OPOP, BIN, UNA, OCALL, ORET, BIND, OREQ, WHERE, OUT, IN, PLIST, 
	ORANGE, PULL, JMP } 
	opcode;

static char* opcode_string[] = {
	"push", "pop", "bin", "una", "call", "ret", "bind", "req", 
	"where", "out", "in", "plist", "range", "pull", "jmp"
};

// generate_code(ast, size) generates Wendy ByteCode based on the ast and 
//   returns the ByteArray as well as the length in the size variable.
// effects: allocates memory, caller must free
uint8_t* generate_code(statement_list* ast, size_t* size);

// print_bytecode(bytecode, size, buffer) prints the given bytecode into 
//   a readable format into the buffer.
void print_bytecode(uint8_t* bytecode, size_t size, FILE* buffer);

// get_token(bytecode, end) gets a token from the bytecode stream
token get_token(uint8_t* bytecode, unsigned int* end);
#endif
