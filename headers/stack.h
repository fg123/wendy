#ifndef STACK_H
#define STACK_H

#include "token.h"
#include <stdbool.h>
// Token Expression Stack Implementation in C
typedef	struct token_stack token_stack;
struct token_stack {
	token* val;
	token_stack* next;
};

// push(token, stack) adds a token to the stack
token_stack* push(token* new_item, token_stack* stack);

// pop(stack) removes the top element of the stack
token_stack* pop(token_stack* stack);

// is_empty(stack) returns true if the stack is empty
bool is_empty(token_stack* stack);

// print_token_stack(stack) prints out the stack
void print_token_stack(const token_stack* stack);

// free_stack(stack) frees all the allocated memory from the stack
void free_stack(token_stack* stack);
#endif
