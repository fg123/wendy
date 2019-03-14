#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

// scanner.h - Felix Guo
// This module tokenizes a string of text input and converts it to a list of
//   tokens, which are passed into [AST] to create a syntax tree.

// scan_tokens(source) creates a list of tokens from the source
int scan_tokens(char* source_, struct token** destination, size_t* alloc_size);

// print_token_list() prints the list of tokens
void print_token_list(struct token* tokens, size_t size);


#endif
