#ifndef OPERATORS_H
#define OPERATORS_H

#include "global.h"
#include "token.h"
#include <stdio.h>

// operators.h - Felix Guo
// This module manages operators in bytecode and in VM

enum vm_operator {
	O_ADD, O_SUB, O_MUL, O_DIV, O_REM, O_IDIV, O_AND, O_OR, O_RANGE, O_NEQ,
	O_EQ, O_IN, O_COPY, O_GT, O_LT, O_GTE, O_LTE, O_NOT, O_NEG, O_MEMBER,
	O_SUBSCRIPT, O_ASSIGN, O_MOD_EQUAL, O_SPREAD, O_POWER
};

#define OPERATOR_STRING \
	"+", "-", "*", "/", "%", "\\", "and", "or", "->", "!=",\
	"==", "~", "~", ">", "<", ">=", "<=", "!", "-", ".", "[", "=", "%=", "...",\
	"^"

extern const char* operator_string[];

enum vm_operator token_operator_binary(struct token op);

enum vm_operator token_operator_unary(struct token op);

#endif
