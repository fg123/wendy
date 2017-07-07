#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdio.h>
#include "global.h"

// token.h - Felix Guo
// This module provides utilities for creating and manipulating tokens for the
//   [scanner].

bool last_printed_newline;

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
	OP(T_ASSIGN_PLUS)\
	OP(T_ASSIGN_MINUS)\
	OP(T_ASSIGN_STAR)\
	OP(T_ASSIGN_SLASH)\
	OP(T_ASSIGN_INTSLASH)\
	OP(T_SEMICOLON) \
	OP(T_SLASH) \
	OP(T_STAR) \
	OP(T_COLON) \
	OP(T_AT) \
	OP(T_HASH) \
	OP(T_PERCENT) \
	OP(T_INTSLASH) \
	OP(T_AMPERSAND) \
	OP(T_TILDE) \
	OP(T_RANGE_OP) \
	OP(T_U_STAR) \
	OP(T_U_MINUS) \
	OP(T_U_TILDE) \
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
	OP(T_MEMBER) \
	OP(T_STRING) \
	OP(T_NUMBER) \
	OP(T_ADDRESS) \
	OP(T_FUNCTION) \
	OP(T_CLOSURE) \
	OP(T_LIST) \
	OP(T_LIST_HEADER) \
	OP(T_RANGE) \
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
	OP(T_SET) \
	OP(T_LOOP) \
	OP(T_LOOP_CONTENT) \
	OP(T_DEFFN) \
	OP(T_RET) \
	OP(T_INPUT) \
	OP(T_INC) \
	OP(T_DEC) \
	OP(T_STRUCT) \
	OP(T_PRINTSTACK) \
	OP(T_REQ) \
	OP(T_EXPLODE) \
	OP(T_TIME) \
	OP(T_STRUCT_HEADER) \
	OP(T_STRUCT_NAME) \
	OP(T_STRUCT_METADATA) \
	OP(T_STRUCT_STATIC) \
	OP(T_STRUCT_PARAM) \
	OP(T_STRUCT_INSTANCE) \
	OP(T_STRUCT_INSTANCE_HEAD) \
	OP(T_STRUCT_PARENT) \
	OP(T_STRUCT_BASE_INSTANCE) \
	OP(T_STRUCT_FUNCTION) \
	OP(T_NONE) \
	OP(T_NONERET) \
	OP(T_CALL) \
	OP(T_PUSH) \
	OP(T_POP) \
	OP(T_COND)

#define ENUM(x) x,
#define STRING(x) #x,

typedef enum {
	FOREACH_TOKEN(ENUM)
} token_type;

static const char *token_string[] = {
    FOREACH_TOKEN(STRING)
};

typedef union {
	double number;
	char string[MAX_STRING_LEN];
} data;

typedef struct {	
	token_type t_type;
	int t_line;
	int t_col;
	data t_data;
} token;

// make_token(t, d) returns a new token
token make_token(token_type t, data d);

// make_data_int(i) makes a data union with the integer provided
data make_data_num(double i);

// makeDataStr(s) makes a data union with the string provided
data make_data_str(char* s);

// none_token() returns a none token
token none_token();

// false_token() returns a false token
token false_token();

// true_token() returns a true token
token true_token();

// time_token() returns the current time in a number token
token time_token();

// empty_token() returns an empty token
token empty_token();

// noneret_token() returns a new noneret_token
token noneret_token();

// list_header_token(size) returns a new list_header token
token list_header_token(int size);

// range_token(start, end) returns a new range token
token range_token(int start, int end);
int range_start(token r);
int range_end(token r);

// get_array_size(t) returns the size of the array given t is an array header.
//int get_array_size(token t);

// print_token(t) prints the value of the token to the string
void print_token(const token* t);

// print_token_inline(T) prints the value of the token inline
void print_token_inline(const token* t, FILE* buf);

// token_equal(a, b) returns true if both tokens are equal and false otherwise
bool token_equal(token* a, token* b);

// precedence(op) returns the precedece of the operator
int precedence(token op);

// is_numeric(tok) returns true if the token holds a numeric value and false
//   otherwise.
bool is_numeric(token tok);


void set_make_token_param(int l, int c); 
#endif
