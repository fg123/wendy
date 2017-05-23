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
// 0x02 | ADD	 |           | pop two wendos from the stack and performs
//                               an add operation, pushing the result back
// 0x03 | SUB	 |			 | pops a then b, and performs b - a
// 0x04	| MUL	 |		     | multiplies
// 0x05 | DIV	 |		     | divides
// 0x06 | IDIV   | 			 | integer divides
// 0x07 | CALL	 | [address] | moves the instruction pointer to the given
//                               address while saving the next instruction
//                               in the stack
// 0x08 | RET	 |           | returns out of the function
// 0x09 | GR     |           | greater than, stack operation pushes true
//                               or false on the stack
// 0x0A | GRE    |           | greater than or equal to
// 0x0B | LT     |           | less than
// 0x0C | LTE    |           | less than or equal to
// 0x0D | EQ     |           | equal to
// 0x0E | NEQ    |           | not equal to
// 0x0F | AND    |           | boolean and
// 0x10 | OR     |           | boolean or
// 0x11 | NEG    |           | negates the top of the stack
// 0x12 | BIND	 | [string]  | binds the identifier to the given memory register
// 0x13 | REQ	 | [size]    | requests memory and stores pointer in memory
//                               register
// 0x14 | WHERE  | [string]  | get memory location of the identifier and stores
//                               in memory register
// 0x15 | OUT    |           | prints out the token at memory register
// 0x16 | IN     |           | saves the token at memory register

// generate_code(ast, size) generates Wendy ByteCode based on the ast and 
//   returns the ByteArray as well as the length in the size variable.
// effects: allocates memory, caller must free
uint8_t* generate_code(statement_list* ast, size_t* size);



#endif
