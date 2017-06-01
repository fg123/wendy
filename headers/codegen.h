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
// DATA TYPE     | DATA (BYTE[s] USED)
// =============================================================================
// [token]       | t_type(1) t_data.number(8)          [if numeric type]
//               | t_type(1) t_data.string(strlen + 1) [any other type]
// [optoken]     | same as [token]
// [string]      | null terminated string
// [size]        | uint8_t(1)
// [address]     | uint32_t(4)
// [number]      | uint8_t(1)
// =============================================================================
// BYTE | OPCODE | PARAMETER | EXPLANATION
// =============================================================================
// 0x00 | PUSH   | [token]   | pushes a wendo on the stack
// 0x01 | POP    |           | pops the wendo at the top of the stack
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
// 0x0A | OUTL   |           | prints without newline
// 0x0B | IN     |           | saves the token at memory register
// 0x0C | PLIST  | [size]    | pops the top n elements into memory locations
//                               in memory register + size, and leaves a ref
// 0x0D | RANGE  |           | creates a range object from top two elements
// 0x0E | READ   |           | pulls wendo from memory register to stack
// 0x0F | WRITE  |           | writes top of stack to memory register
// 0x10 | JMP    | [address] | jumps to the given address in bytecode
// 0x11 | MADD   | [number]  | adds n to the memory register
// 0x12 | MSUB   | [number]  | subtracts n from the memory register
// 0x13 | JIF    | [address] | if top of stack is false, jumps to address
// 0x14 | FRM    |           | pushes an auto local variable frame
// 0x15 | END    |           | pops a local variable frame
// 0x16 | LJMP   | [address] | given loop index var name, performs check if 
//                 [string]  |   loop is done, if it is jump to the address
// 0x17 | LBIND  | [string]  | first is userindexvar, second is loop index var
//                 [string]  |
// 0x18 | INC    |           | increments value at memory register
// 0x19 | DEC    |           | decrements value at memory register
// 0x1A | NTHPTR |           | list at mem register, and a number index 
//                               off top of the stack and puts an address in the
//                               memory register
// 0x1B | MEMPTR |           | struct/structinstance at mem_register and a 
//								 member id at top of stack and puts an address 
//								 in the memory register

typedef enum opcode { 
	OPUSH, OPOP, BIN, UNA, OCALL, ORET, BIND, OREQ, WHERE, OUT, OUTL, IN, PLIST, 
	ORANGE, READ, WRITE, JMP, MADD, MSUB, JIF, FRM, END, LJMP, LBIND, OINC, 
	ODEC, NTHPTR, MEMPTR } 
	opcode;

static char* opcode_string[] = {
	"push", "pop", "bin", "una", "call", "ret", "bind", "req", 
	"where", "out", "outl", "in", "plist", "range", "read", "write", "jmp", "madd", 
	"msub", "jif", "frm", "end", "ljmp", "lbind", "inc", "dec", "nthptr",
	"memptr"
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
