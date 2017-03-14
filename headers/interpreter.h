#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "token.h"
#include <stdlib.h>
#include "stack.h"
#include <stdbool.h>
#include "memory.h"


// interpreter.h provides the standard functions for running the interpreter
//   such as evaluations. 
// ID handling is done via memory.h

// parse_line(start, size, i_ptr) parses a command statement
bool parse_line(address start, size_t size, int* i_ptr);

// run(input_string) processes the given input string in WendyScript
void run(char* input_string);

// eval(start, size) evaluates the tokens into one expression (within a line)
token eval(address start,  size_t size);

// eval_binop(op, a, b) applies the given operator to a and b
//  a and b must be numbers or boolean
token eval_binop(token op, token a, token b);

// eval_uniop(op, a) applies the given operator to a
//   a must be a number or boolean
token eval_uniop(token op, token a);

// eval_identifier(address start, size_t size) given a string of tokens, 
//   evaluates it as an identifier, including pointers and arrays and returns
//   the memory space it points to.
address eval_identifier(address start, size_t size);

// eval_one_expr(i, expr_stack, search_end) calls when you have found a right
//   bracket or parentheses,
void eval_one_expr(address i, token_stack** expr_stack, token_type search_end, 
		bool entire_line);
#endif
