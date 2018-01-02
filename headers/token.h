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
    OP(T_SET) \
    OP(T_LOOP) \
    OP(T_DEFFN) \
    OP(T_RET) \
    OP(T_INPUT) \
    OP(T_INC) \
    OP(T_DEC) \
    OP(T_STRUCT) \
    OP(T_REQ) \
    OP(T_NATIVE) \
    OP(T_NONE) \
    OP(T_NONERET) \
    OP(T_MEMBER) \
	OP(T_DOLLAR_SIGN)

typedef enum {
    FOREACH_TOKEN(ENUM)
} token_type;

static const char *token_string[] = {
    FOREACH_TOKEN(STRING)
};

typedef union {
    double number;
    char string[MAX_STRING_LEN];
} token_data;

typedef struct {
    token_type t_type;
    int t_line;
    int t_col;
    token_data t_data;
} token;

// make_token(t, d) returns a new token
token make_token(token_type t, token_data d);

// make_data_int(i) makes a data union with the integer provided
token_data make_data_num(double i);

// makeDataStr(s) makes a data union with the string provided
token_data make_data_str(char* s);

// none_token() returns a none token
token none_token();

// none_ret() returns a noneret token
token noneret_token();

// false_token() returns a false token
token false_token();

// true_token() returns a true token
token true_token();

// empty_token() returns an empty token
token empty_token();

// print_token(t) prints the value of the token to the string
void print_token(const token* t);

// print_token_inline(T) prints the value of the token inline and returns # of
//   characters printed
unsigned int print_token_inline(const token* t, FILE* buf);

// precedence(op) returns the precedece of the operator
int precedence(token op);

// set_make_token_param(l, c) sets the line and column of the current state of
//   the scanner
void set_make_token_param(int l, int c);
#endif
