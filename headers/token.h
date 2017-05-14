#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdio.h>
#include "macros.h"

bool last_printed_newline;

typedef enum {
	EMPTY,
	// Single-character Tokens
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE, LEFT_BRACK, RIGHT_BRACK,
	COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR, COLON, AT, HASH, PERCENT,
	INTSLASH, AMPERSAND, TILDE, RANGE_OP,

	// Special Expression Token (Unary) (22)
	U_STAR, U_MINUS, U_TILDE,

	// Comparison Tokens (24)
	NOT, NOT_EQUAL,
	EQUAL, EQUAL_EQUAL, 
	GREATER, GREATER_EQUAL,
	LESS, LESS_EQUAL, 
	
	// #: token (32)
	LAMBDA,

	// Block Comments (/* and */)
	B_COMMENT_START, B_COMMENT_END,

	// Literals / Primitives
	IDENTIFIER, STRING, NUMBER, ADDRESS, FUNCTION, 

	// Closures
	CLOSURE, 

	// Lists and Ranges 
	LIST, LIST_HEADER, RANGE,

	// Object Types
	OBJ_TYPE, 

	// Conditionals
	AND, OR, IF, ELSE, ELSEIF, TRUE, FALSE,

	// Definitons
	LET, SET, LOOP, LOOP_CONTENT, DEFFN,
	
	// Special Commands
	RET, INPUT, INC, DEC, STRUCT, PRINTSTACK, REQ, EXPLODE,
	TIME, ASSERT,

	// Struct Forms:
	STRUCT_HEADER, STRUCT_NAME, STRUCT_METADATA, STRUCT_STATIC, STRUCT_PARAM,
	STRUCT_INSTANCE, STRUCT_INSTANCE_HEAD, STRUCT_PARENT, STRUCT_BASE_INSTANCE, 

	// Special Forms
	NONE, NONERET,
	
	// Preprocessor Specific Symbols
	CALL, PUSH, POP, COND

} token_type;

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
#endif
