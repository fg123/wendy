#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

// scanner.h provides tools to convert a code string into a list of tokens.

// scan_tokens(source) creates a list of tokens from the source
int scan_tokens(char* source_);

// copy_token(destination) actually copies the tokens over
void copy_tokens(token* destination);

// scan_token() processes the next token
void scan_token();

// add_token(type) just adds the given token 
void add_token(token_type type);

// add_token_V(type, val) adds the given token to the list with the given 
//   val
void add_token_V(token_type type, data val);

// print_token_list() prints the list of tokens
void print_token_list(token* tokens);

// error(line, message) prints an error message to the screen
void error(int line, char* message);


#endif
