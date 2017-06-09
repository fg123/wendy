#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

// codegen.h - Felix Guo
// This module generates the bytecode that runs on the WendyVM, based on the
//   abstract syntax tree that is passed in from the [ast] module.

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
// BYTE | OPCODE | PARAMETER | STACK CHANGE
// > EXPLANATION
// =============================================================================
// 0x00 | PUSH   | [token]   | (...) -> (...) [token]
// 0x01 | POP    |           | (...) [token] -> (...)
// 0x02 | BIN	 | [optoken] | (...) (a) (b) -> (...) (a OP b)
// 0x03 | UNA	 | [optoken] | (...) (a) -> (...) (OP a)
// 0x04 | CALL	 |           | (...) [address] -> (...)
//	 `- moves the instruction pointer to the given address with new callstack
// 0x05 | RET	 |           | 
//   `- moves instruction pointer back to caller
// 0x06 | BIND	 | [string]  | 
//   `- binds the identifier to the $MR
// 0x07 | REQ	 | [size]    | 
//   `- requests memory and stores pointer in $MR
// 0x08 | WHERE  | [string]  | 
//   `- get memory location of the identifier and stores in $MR
// 0x09 | OUT    |           | (...) [token] -> (...)
//   `- pops and prints token at top of stack
// 0x0A | OUTL   |           | (...) [token] -> (...)
//   `- prints token out without a newline
// 0x0B | IN     |           | 
//   `- promps user input, then saves userinput into token at $MR
// 0x0C | PLIST  | [size]    | (...) (a1) ... (an) -> (...) [list_token]
//   `- pops the top [size] elements into memory location $MR
// 0x0E | READ   |           | (...) -> (...) (a)
//   `- pulls token from $MR to stack
// 0x0F | WRITE  |           | (...) (a) -> (...)
//   `- writes top of stack to $MR
// 0x10 | JMP    | [address] | 
//   `- jumps to the given address in bytecode
// 0x11 | MADD   | [number]  | adds n to the memory register
// 0x12 | MSUB   | [number]  | subtracts n from the memory register
// 0x13 | JIF    | [address] | 
//   `- if top of stack is false, jumps to address
// 0x14 | FRM    |           |
//   `- creates a local variable stack frame
// 0x15 | END    |           | 
//	 `- pops a local variable stack frame
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
// 0x1C | ASSERT | [toktype] | verifies that the type at the memory register is
//                 [string]  |   what it should be, prints error if not
// 0x1D | MPTR   |           | sets memory_register to number of the item at
//                               memory_register
// 0x1E | DIN    |           | moves data from element in top of the stack to
//                               data register
// 0x1F | DOUT   |           | moves data from data register to top of the 
//                               stack
// 0x20 | CLOSUR |           | binds a closure, pushes a closure to the top of
//                               the stack.
// 0x21 | RBIN   |           | reverse binary operator, b OP a
// 0x22 | RBW    | [string]  | REQ-BIND-WRITE, requests 1, binds string 
// 0x23 | CHTYPE | [toktype] | changes the type of the top of the stack
// 0x24 | HALT   |           | halt instruction, end program

typedef enum opcode { 
	OPUSH, OPOP, BIN, UNA, OCALL, ORET, BIND, OREQ, WHERE, OUT, OUTL, IN, PLIST, 
	ORANGE, READ, WRITE, JMP, MADD, MSUB, JIF, FRM, END, LJMP, LBIND, OINC, 
	ODEC, NTHPTR, MEMPTR, ASSERT, MPTR, DIN, DOUT, CLOSUR, RBIN, RBW, CHTYPE, 
	HALT } 
	opcode;

static char* opcode_string[] = {
	"push", "pop", "bin", "una", "call", "ret", "bind", "req", 
	"where", "out", "outl", "in", "plist", "range", "read", "write", "jmp", "madd", 
	"msub", "jif", "frm", "end", "ljmp", "lbind", "inc", "dec", "nthptr",
	"memptr", "assert", "mptr", "din", "dout", "closur", "rbin", "rbw", "chtype",
	"halt"
};

// generate_code(ast) generates Wendy ByteCode based on the ast and 
//   returns the ByteArray
// effects: allocates memory, caller must free
uint8_t* generate_code(statement_list* ast);

// print_bytecode(bytecode, buffer) prints the given bytecode into 
//   a readable format into the buffer.
void print_bytecode(uint8_t* bytecode, FILE* buffer);

// write_bytecode(bytecode, buffer) writes the bytecode into binary file
void write_bytecode(uint8_t* bytecode, FILE* buffer);

// get_token(bytecode, end) gets a token from the bytecode stream
token get_token(uint8_t* bytecode, unsigned int* end);

// verify_header(bytecode) checks the Header for Information, 
//   then returns the index of the start
int verify_header(uint8_t* bytecode);

#endif
