#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
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
//
// VM Registers: $MR -> Memory Pointer Register
// =============================================================================
// DATA TYPE     | DATA (BYTE[s] USED)
// =============================================================================
// [token]       | t_type(1) t_data.number(8)          [if numeric type]
//               | t_type(1) t_data.string(strlen + 1) [any other type]
// [op]          | one byte of op
// [string]      | null terminated string
// [size]        | uint8_t(1)
// [address]     | uint32_t(4)
// [number]      | uint8_t(1)
// [int32]       | uint32_t(4)
// =============================================================================
// BYTE | OPCODE | PARAMETER | STACK CHANGE
//   `- EXPLANATION
// =============================================================================
// 0x00 | PUSH   | [token]   | (...) -> (...) [token]
// 0x01 | POP    |           | (...) [token] -> (...)
// 0x02 | BIN    | [op]      | (...) (a) (b) -> (...) (a OP b)
// 0x03 | UNA    | [op]      | (...) (a) -> (...) (OP a)
// 0x04 | CALL   |           | (...) [address] -> (...)
//   `- moves the instruction pointer to the given address with new callstack
// 0x05 | RET    |           |
//   `- moves instruction pointer back to caller
// 0x06 | BIND   | [string]  |
//   `- binds the identifier to the $MR
// 0x07 | REQ    | [size]    |
//   `- requests memory and stores pointer in $MR
// 0x08 | WHERE  | [string]  |
//   `- get memory location of the identifier and stores in $MR
// 0x09 | OUT    |           | (...) [token] -> (...)
//   `- pops and prints token at top of stack
// 0x0A | OUTL   |           | (...) [token] -> (...)
//   `- prints token out without a newline
// 0x0B | IN     |           |
//   `- promps user input, then saves userinput into token at $MR
// 0x0C | MKPTR  | [toktype] | (...) -> (...) [list_token]
//   `- pushes a ref with toktype pointing to memory location $MR
// 0x0E | READ   |           | (...) -> (...) (a)
//   `- pulls token from $MR to stack
// 0x0F | WRITE  | [size]    | (...) (a) -> (...)
//   `- writes top [size] of stack to $MR
// 0x10 | JMP    | [address] |
//   `- jumps to the given address in bytecode
// 0x11 | JIF    | [address] |
//   `- if top of stack is false, jumps to address
// 0x12 | FRM    |           |
//   `- creates a local variable stack frame
// 0x13 | END    |           |
//   `- pops a local variable stack frame
// 0x14 | LJMP   | [address] | given loop index var name, performs check if
//                 [string]  |   loop is done, if it is jump to the address
// 0x15 | LBIND  | [string]  | first is userindexvar, second is loop index var
//                 [string]  |
// 0x16 | INC    |           | increments value at memory register
// 0x17 | DEC    |           | decrements value at memory register
// 0x18 | NTHPTR |           | list at mem register, and a number index
//                               off top of the stack and puts an address in the
//                               memory register
// 0x19 | MEMPTR |           | struct/structinstance at mem_register and a
//                               member id at top of stack and puts an address
//                               in the memory register
// 0x1A | ASSERT | [toktype] | verifies that the type at the memory register is
//                 [string]  |   what it should be, prints error if not
// 0x1B | MPTR   |           | sets memory_register to number of the item at
//                               memory_register
// 0x1C | DIN    |           | moves data from element in top of the stack to
//                               data register
// 0x1D | DOUT   |           | moves data from data register to top of the
//                               stack
// 0x1E | CLOSUR |           | binds a closure, pushes a closure to the top of
//                               the stack.
// 0x1F | RBIN   | [op]      | reverse binary operator, b OP a
// 0x20 | RBW    | [string]  | REQ-BIND-WRITE, requests 1, binds string
// 0x21 | CHTYPE | [toktype] | changes the type of the top of the stack
// 0x22 | HALT   |           | halt instruction, end program
// 0x23 | SRC    | [int32]   | store the src line number
// 0x24 | NATIVE | [int32]   | binds to native function
//                 [string]
// 0x25 | IMPORT | [string]  | denotes that the library was imported
//               | [address] |   jumps if already imported

typedef enum opcode {
    OP_PUSH, OP_POP, OP_BIN, OP_UNA, OP_CALL, OP_RET, OP_BIND, OP_REQ, OP_WHERE,
    OP_OUT, OP_OUTL, OP_IN, OP_MKPTR, OP_RANGE, OP_READ, OP_WRITE, OP_JMP,
    OP_JIF, OP_FRM, OP_END, OP_LJMP, OP_LBIND, OP_INC, OP_DEC, OP_NTHPTR,
    OP_MEMPTR, OP_ASSERT, OP_MPTR, OP_DIN, OP_DOUT, OP_CLOSUR, OP_RBIN,
    OP_RBW, OP_CHTYPE, OP_HALT, OP_SRC, OP_NATIVE, OP_IMPORT }
    opcode;

static char* opcode_string[] = {
    "push", "pop", "bin", "una", "call", "ret", "bind", "req",
    "where", "out", "outl", "in", "mkptr", "range", "read", "write", "jmp",
    "jif", "frm", "end", "ljmp", "lbind", "inc", "dec", "nthptr",
    "memptr", "assert", "mptr", "din", "dout", "closur", "rbin", "rbw", "chtype",
    "halt", "src", "native", "import"
};

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
