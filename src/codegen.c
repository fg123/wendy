#include "codegen.h"
#include "ast.h"
#include "macros.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "token.h"

// Implementation of Wendy ByteCode Generator

static uint8_t* bytecode = 0;
static size_t capacity = 0;
static size_t size = 0;


void guarantee_size() {
	if (size + CODEGEN_PAD_SIZE	> capacity) {
		capacity *= 2;
		uint8_t* re = realloc(bytecode, capacity * sizeof(uint8_t));
		if (!re) {
			w_error(REALLOC_ERROR);
		}
		bytecode = re;
	}
}

// writes the token to the bytestream, guaranteed to be a literal
void write_token(token t) {
	bytecode[size++] = t.t_type;
	if (t.t_type == NUMBER) {
		// Writing a double
		unsigned char * p = &t.t_data.number;
		for (int i = 0; i < sizeof(double); i++) {
			bytecode[size++] = p[i];
		}
	}
	else {
		size_t length = strlen(t.t_data.string);
		for (int i = 0; i < length; i++) {
			bytecode[size++] = t.t_data.string[i];
		}
		// Null Terminator
		bytecode[size++] = 0;
	}
	guarantee_size();
}

void codegen_expr(void* expre) {
	expr* expression = (expr*)expre;
	if (expression->type == E_LITERAL) {
		// Literal Expression, we push to the stack.
		write_token(expression->op.lit_expr);
	}
	else if (expression->type == E_BINARY) {
		printf("(%p) Binary Expression (%p, %p)\n", expre,
			expression->op.bin_expr.left, expression->op.bin_expr.right);
	}
	else if (expression->type == E_UNARY) {
		printf("Unary Expression\n");
	}
	else if (expression->type == E_CALL) {
		printf("Call Expression\n");
	}
	else if (expression->type == E_LIST) {
		printf("List Expression\n");
	}
	else if (expression->type == E_FUNCTION) {
		printf("Function Expression\n");
	}
	else {
		printf("Traverse Expr: Unknown Type\n");
	}
}



uint8_t* generate_code(statement_list* _ast, size_t* _size) {

	// reset
	capacity = CODEGEN_START_SIZE;
	bytecode = malloc(capacity * sizeof(uint8_t));
	size = 0;
	traverse_ast(_ast, codegen_expr, codegen_expr_list, codegen_statement,
		codegen_statement_list, false);

	*_size = size;
	return bytecode;
}
