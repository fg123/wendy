#ifndef VM_H
#define VM_H
#include "token.h"
#include "stdint.h"

// vm_run(bytecode) runs the given bytecode.
void vm_run(uint8_t* bytecode);

// eval_binop(op, a, b) applies the given operator to a and b
//  a and b must be numbers or boolean
static token eval_binop(token op, token a, token b);

// eval_uniop(op, a) applies the given operator to a
//   a must be a number or boolean
static token eval_uniop(token op, token a);

// type_of(a) returns the type of the token a in an OBJ_TYPE token.
static token type_of(token a);

// size_of(a) returns the size of the token a in a NUMBER token.
static token size_of(token a);
#endif
