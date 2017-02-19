#ifndef TOKEN_H
#define TOKEN_H

#define MAX_STRING_LEN 1024

#include <stdbool.h>

bool last_printed_newline;

typedef enum token_type_t { 
	// Single-character Tokens
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE, LEFT_BRACK, RIGHT_BRACK,
	COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR, COLON, AT, HASH, PERCENT,
	INTSLASH, AMPERSAND,

	// Comparison Tokens
	NOT, NOT_EQUAL,
	EQUAL, EQUAL_EQUAL, 
	GREATER, GREATER_EQUAL,
	LESS, LESS_EQUAL,

	// Literals.
	IDENTIFIER, STRING, NUMBER,

	// Conditionals
	AND, OR, IF, ELSE, ELSEIF, TRUE, FALSE,

	// Definitons
	LET, SET, LOOP, DEFCOLON, DEFFN,
	
	// Special Commands
	RET, INPUT, INC, DEC, STRUCT, REQ, ERR, MEMSET, PRINTSTACK,

	// Special Forms
	NONE, EOFILE, CONDSCOPE, ARRAYREF, NONEPTR, NONERET
} token_type;

typedef union {
	double number;
	char string[MAX_STRING_LEN];
} data;

typedef struct {
	token_type t_type;
	data t_data;
	int t_line;
} token;

// make_token(t, d) returns a new token
token make_token(token_type t, data d);

// make_data_int(i) makes a data union with the integer provided
data make_data_num(double i);

// makeDataStr(char*) makes a data union with the string provided
data make_data_str(char* s);

// none_token() returns a none token
token none_token();

// false_token() returns a false token
token false_token();

// true_token() returns a true token
token true_token();

// print_token(t) prints the value of the token to the string
void print_token(const token* t);

// print_token_inline(T) prints the value of the token inline
void print_token_inline(const token* t);
#endif
