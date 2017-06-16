#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

// scanner.h - Felix Guo
// This module tokenizes a string of text input and converts it to a list of
//   tokens, which are passed into [ast] to create a syntax tree.


// scan_tokens(source) creates a list of tokens from the source
int scan_tokens(char* source_, token** destination, size_t* alloc_size);

// scan_token() processes the next token
static bool scan_token();

// add_token(type) just adds the given token 
//static void add_token(token_type type);

// add_token_V(type, val) adds the given token to the list with the given 
//   val
//static void add_token_V(token_type type, data val);

// print_token_list() prints the list of tokens
void print_token_list(token* tokens, size_t size);


#endif
