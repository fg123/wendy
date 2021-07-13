#ifndef TOKEN_H
#define TOKEN_H

#include "global.h"
#include <stdbool.h>
#include <stdio.h>

// token.h - Felix Guo
// This module provides utilities for creating and manipulating tokens for the
//   [scanner].

#define FOREACH_TOKEN(OP) \
	OP(T_EMPTY) \
	OP(T_LEFT_PAREN) \
	OP(T_RIGHT_PAREN) \
	OP(T_LEFT_BRACE) \
	OP(T_RIGHT_BRACE) \
	OP(T_LEFT_BRACK) \
	OP(T_RIGHT_BRACK) \
	OP(T_DEBUG) \
	OP(T_COMMA) \
	OP(T_DOT) \
	OP(T_MINUS) \
	OP(T_PLUS) \
	OP(T_MOD_EQUAL) \
	OP(T_ASSIGN_PLUS) \
	OP(T_ASSIGN_MINUS) \
	OP(T_ASSIGN_STAR) \
	OP(T_ASSIGN_SLASH) \
	OP(T_ASSIGN_INTSLASH) \
	OP(T_SEMICOLON) \
	OP(T_SLASH) \
	OP(T_STAR) \
	OP(T_COLON) \
	OP(T_AT) \
	OP(T_HASH) \
	OP(T_PERCENT) \
	OP(T_INTSLASH) \
	OP(T_TILDE) \
	OP(T_RANGE_OP) \
	OP(T_NOT) \
	OP(T_NOT_EQUAL) \
	OP(T_EQUAL) \
	OP(T_EQUAL_EQUAL) \
	OP(T_GREATER) \
	OP(T_GREATER_EQUAL) \
	OP(T_LESS) \
	OP(T_LESS_EQUAL) \
	OP(T_LAMBDA) \
	OP(T_B_COMMENT_START) \
	OP(T_B_COMMENT_END) \
	OP(T_IDENTIFIER) \
	OP(T_STRING) \
	OP(T_NUMBER) \
	OP(T_OBJ_TYPE) \
	OP(T_AND) \
	OP(T_OR) \
	OP(T_IF) \
	OP(T_IN) \
	OP(T_ELSE) \
	OP(T_ELSEIF) \
	OP(T_TRUE) \
	OP(T_FALSE) \
	OP(T_LET) \
	OP(T_LOOP) \
	OP(T_BREAK) \
	OP(T_CONTINUE) \
	OP(T_DEFFN) \
	OP(T_RET) \
	OP(T_INPUT) \
	OP(T_INC) \
	OP(T_DEC) \
	OP(T_STRUCT) \
	OP(T_ENUM) \
	OP(T_REQ) \
	OP(T_NATIVE) \
	OP(T_NONE) \
	OP(T_MEMBER) \
	OP(T_DOLLAR_SIGN) \
	OP(T_DOT_DOT_DOT) \
	OP(T_CARET) \
	OP(T_INIT)

enum token_type {
	FOREACH_TOKEN(ENUM)
};

extern const char* token_string[];

union token_data {
	double number;
	char* string;
};

struct token {
	enum token_type t_type;
	int t_line;
	int t_col;
	union token_data t_data;
};

// make_token(t, d) returns a new token
struct token make_token(enum token_type t, union token_data d);

// make_data_int(i) makes a data union with the integer provided
union token_data make_data_num(double i);

// makeDataStr(s) makes a data union with the string provided
#define make_data_str(s) make_data_str_impl(safe_strdup((s)))
union token_data make_data_str_impl(char* s);

// makes a deep copy
struct token copy_token(struct token old);

// none_token() returns a none token
struct token none_token(void);

// false_token() returns a false token
struct token false_token(void);

// true_token() returns a true token
struct token true_token(void);

// empty_token() returns an empty token
struct token empty_token(void);

// print_token(t) prints the value of the token to the string
void print_token(const struct token* t);

// print_token_inline(T) prints the value of the token inline and returns # of
//   characters printed
unsigned int print_token_inline(const struct token* t, FILE* buf);

// precedence(op) returns the precedece of the enum vm_operator
int precedence(struct token op);

// set_make_token_param(l, c) sets the line and column of the current state of
//   the scanner
void set_make_token_param(int l, int c);

// destroys_token(t) frees all memory associated with the token
void destroy_token(struct token t);

// free_token_list(l) frees the token list and associated strings
void free_token_list(struct token* l, size_t s);

#endif
