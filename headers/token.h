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
	OP(EMPTY) \
	OP(LEFT_PAREN) \
	OP(RIGHT_PAREN) \
	OP(LEFT_BRACE) \
	OP(RIGHT_BRACE) \
	OP(LEFT_BRACK) \
	OP(RIGHT_BRACK) \
	OP(DEBUG) \
	OP(COMMA) \
	OP(DOT) \
	OP(MINUS) \
	OP(PLUS) \
	OP(ASSIGN_PLUS)\
	OP(ASSIGN_MINUS)\
	OP(ASSIGN_STAR)\
	OP(ASSIGN_SLASH)\
	OP(ASSIGN_INTSLASH)\
	OP(SEMICOLON) \
	OP(SLASH) \
	OP(STAR) \
	OP(COLON) \
	OP(AT) \
	OP(HASH) \
	OP(PERCENT) \
	OP(INTSLASH) \
	OP(AMPERSAND) \
	OP(TILDE) \
	OP(RANGE_OP) \
	OP(U_STAR) \
	OP(U_MINUS) \
	OP(U_TILDE) \
	OP(NOT) \
	OP(NOT_EQUAL) \
	OP(EQUAL) \
	OP(EQUAL_EQUAL) \
	OP(GREATER) \
	OP(GREATER_EQUAL) \
	OP(LESS) \
	OP(LESS_EQUAL) \
	OP(LAMBDA) \
	OP(B_COMMENT_START) \
	OP(B_COMMENT_END) \
	OP(IDENTIFIER) \
	OP(MEMBER) \
	OP(STRING) \
	OP(NUMBER) \
	OP(ADDRESS) \
	OP(FUNCTION) \
	OP(CLOSURE) \
	OP(LIST) \
	OP(LIST_HEADER) \
	OP(RANGE) \
	OP(OBJ_TYPE) \
	OP(AND) \
	OP(OR) \
	OP(IF) \
	OP(INOP) \
	OP(ELSE) \
	OP(ELSEIF) \
	OP(TRUE) \
	OP(FALSE) \
	OP(LET) \
	OP(SET) \
	OP(LOOP) \
	OP(LOOP_CONTENT) \
	OP(DEFFN) \
	OP(RET) \
	OP(INPUT) \
	OP(INC) \
	OP(DEC) \
	OP(STRUCT) \
	OP(PRINTSTACK) \
	OP(REQ) \
	OP(EXPLODE) \
	OP(TIME) \
	OP(STRUCT_HEADER) \
	OP(STRUCT_NAME) \
	OP(STRUCT_METADATA) \
	OP(STRUCT_STATIC) \
	OP(STRUCT_PARAM) \
	OP(STRUCT_INSTANCE) \
	OP(STRUCT_INSTANCE_HEAD) \
	OP(STRUCT_PARENT) \
	OP(STRUCT_BASE_INSTANCE) \
	OP(STRUCT_FUNCTION) \
	OP(NONE) \
	OP(NONERET) \
	OP(CALL) \
	OP(PUSH) \
	OP(POP) \
	OP(COND)

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
#endif
