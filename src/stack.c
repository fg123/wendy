#include "stack.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "token.h"

token_stack* push(token* new_item, token_stack* stack) {
	token_stack* new_stack = malloc(sizeof(token_stack));
	if (new_stack) {
		new_stack->val = new_item;
		new_stack->next = stack;
		stack = new_stack;
		return stack;
	}
	else {
		printf("Cannot Malloc on Stack.\n");
		exit(1);
	}
}

token_stack* pop(token_stack* stack) {
	if (stack) {
		token_stack* t = stack->next;
//		printf("FREEING POINTER\n");
//		free(stack);
//		printf("Free Complete;\n");
		// TODO Should free here free(stack);
		return t;
	}
	else {
		printf("Error: pop: Stack is Empty!\n");
		return stack;
	}
}

bool is_empty(token_stack* stack) {
	return !stack;
}

void print_token_stack(const token_stack* stack) {
	if (!stack) {
		printf("Stack is Empty!\n");
	}
	else {
		token_stack* next = stack->next;

		//printf("Top of Stack:\n");
		print_token(stack->val);
		while (next) {
			print_token(next->val);
			next = next->next;
		}
	}
}

void free_stack(token_stack* stack) {
	if (stack) {
		free_stack(stack->next);
		free(stack);
	}
}
