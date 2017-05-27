#include "ast.h"
#include "macros.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "token.h"
#include "codegen.h"
#include "error.h"
#include <string.h>

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

void write_string(char* string) {
	for (int i = 0; string[i]; i++) {
		bytecode[size++] = string[i];
	}
	bytecode[size++] = 0;
	guarantee_size();
}

// writes the token to the bytestream, guaranteed to be a literal
void write_token(token t) {
	bytecode[size++] = t.t_type;
	if (t.t_type == NUMBER || t.t_type == ADDRESS ||
		t.t_type == LIST_HEADER) {
		// Writing a double
		unsigned char * p = (void*)&t.t_data.number;
		for (int i = 0; i < sizeof(double); i++) {
			bytecode[size++] = p[i];
		}
	}
	else {
		write_string(t.t_data.string);
	}
	guarantee_size();
}

void write_opcode(opcode op) {
	bytecode[size++] = op;
	guarantee_size();
}

// Generates Evaluates lValue expr and stores in memory register
void codegen_lvalue_expr(expr* expression) {
	if (expression->type == E_LITERAL) {
		// Better be a identifier eh
		if (expression->op.lit_expr.t_type != IDENTIFIER) {
			error(expression->op.lit_expr.t_line, LVALUE_EXPECTED_IDENTIFIER);
		}
		write_opcode(WHERE);
		write_string(expression->op.lit_expr.t_data.string);
	}
}
void codegen_expr(void* expre);
void codegen_statement(void* expre);
void codegen_statement_list(void* expre);

void codegen_expr_list(void* expre) {
	expr_list* list = (expr_list*) expre;
	while (list) {
		codegen_expr(list->elem);
		list = list->next;
	}
}

void codegen_statement(void* expre) {
	if (!expre) return;
	statement* state = (statement*) expre;
	if (state->type == S_LET) {
		codegen_expr(state->op.let_statement.rvalue);
		// Request Memory
		write_opcode(OREQ);
		bytecode[size++] = 1;
		write_opcode(OPOP);
		write_opcode(BIND);
		write_string(state->op.let_statement.lvalue.t_data.string);			
	}
	else if (state->type == S_OPERATION) {
		if (state->op.operation_statement.operator.t_type == RET) {
			codegen_expr(state->op.operation_statement.operand);
			write_opcode(ORET);
		}
		else {
			codegen_lvalue_expr(state->op.operation_statement.operand);
			//write_opcode(tok_to_opcode(
			//	state->op.operation_statement.operator, false));
		}
	}
	else if (state->type == S_EXPR) {
		codegen_expr(state->op.expr_statement);
		write_opcode(OUT);
	}
	else if (state->type == S_BLOCK) {
		codegen_statement_list(state->op.block_statement);
	}
	else if (state->type == S_STRUCT) {
		printf("(%p) Struct Statement ", state);
		print_token_inline(&state->op.struct_statement.name, stdout);
		printf(":");
		print_token_inline(&state->op.struct_statement.parent, stdout);
		printf("(%p (%p)\n", state->op.struct_statement.instance_members,
			state->op.struct_statement.static_members);
	}
	else if (state->type == S_IF) {
		printf("(%p) If Statement (%p) T(%p) F(%p)\n", state,
			state->op.if_statement.condition,
			state->op.if_statement.statement_true, 
			state->op.if_statement.statement_false);
	}
	else if (state->type == S_LOOP) {
			printf("(%p) Loop Statement (%p) T(%p)\n", state, 
			state->op.loop_statement.condition,
			state->op.loop_statement.statement_true);
	}
	else {
		printf("Traverse Statement: Unknown Type\n");
	}

}
void codegen_statement_list(void* expre) {
	statement_list* list = (statement_list*) expre;
	while (list) {
		codegen_statement(list->elem);
		list = list->next;
	}	
}
void codegen_expr(void* expre) {
	if (!expre) return;
	expr* expression = (expr*)expre;
	if (expression->type == E_LITERAL) {
		// Literal Expression, we push to the stack.
		write_opcode(OPUSH);
		write_token(expression->op.lit_expr);
	}
	else if (expression->type == E_BINARY) {
		codegen_expr(expression->op.bin_expr.left);
		codegen_expr(expression->op.bin_expr.right);
//		write_opcode(tok_to_opcode(expression->op.bin_expr.operator, false));
		write_opcode(BIN);
		write_token(expression->op.bin_expr.operator);
	}
	else if (expression->type == E_UNARY) {
		codegen_expr(expression->op.una_expr.operand);
//		write_opcode(tok_to_opcode(expression->op.una_expr.operator, false));
		write_opcode(UNA);
		write_token(expression->op.una_expr.operator);
	}
	else if (expression->type == E_CALL) {
		codegen_expr_list(expression->op.call_expr.arguments);
		codegen_lvalue_expr(expression->op.call_expr.function);
		write_opcode(PULL);
		write_opcode(OCALL);
	}
	else if (expression->type == E_LIST) {
		int count = expression->op.list_expr.length;
		write_opcode(OREQ);
		// For the Header
		bytecode[size++] = count + 1;
		write_opcode(OPUSH);
		write_token(make_token(LIST_HEADER, make_data_num(count)));
		expr_list* param = expression->op.list_expr.contents;
		while (param) {
			codegen_expr(param->elem);
			param = param->next;
		}
		write_opcode(PLIST);
		bytecode[size++] = count + 1;
	}
	else if (expression->type == E_FUNCTION) {
		write_opcode(JMP);
		int writeSizeLoc = size++;
		int startAddr = size;
		// Parameters are reversed from AST Generation
		expr_list* param = expression->op.func_expr.parameters;
		while (param) {
			token t = param->elem->op.lit_expr;
			write_opcode(OREQ);
			bytecode[size++] = 1;
			write_opcode(BIND);
			write_string(t.t_data.string);
			write_opcode(OPOP);
			param = param->next;
		}
		codegen_statement_list(expression->op.func_expr.body);
		bytecode[writeSizeLoc] = size;
		write_opcode(OPUSH);
		write_token(make_token(ADDRESS, make_data_num(startAddr)));
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
	codegen_statement_list(_ast);
	*_size = size;
	return bytecode;
}

token get_token(uint8_t* bytecode, unsigned int* end) {
	token t;
	int start = *end;
	int i = 0;
	t.t_type = bytecode[i++];
	t.t_data.string[0] = 0;
	if (t.t_type == NUMBER || t.t_type == ADDRESS ||
		t.t_type == LIST_HEADER) {
		unsigned char * p = (void*)&t.t_data.number;
		for (int j = 0; j < sizeof(double); j++) {
			p[j] = bytecode[i++];
		}
	}
	else {
		int len = 0;
		while (bytecode[i++]) {
			t.t_data.string[len++] = bytecode[i - 1];
		}
		// i is 1 past the null term
		t.t_data.string[len++] = 0;
	}
	*end = start + i - 1;
	return t;
}
void print_bytecode(uint8_t* bytecode, size_t size, FILE* buffer) {
	fprintf(buffer, "WendyVM ByteCode Disassembly %zd\n", size);
	for (unsigned int i = 0; i < size; i++) {
		unsigned int start = i;
		fprintf(buffer, "  <%p> <+%04X>: ", &bytecode[i], i);
		opcode op = bytecode[i];
		long int oldChar = ftell(buffer);
		fprintf(buffer, "%s ", opcode_string[op]);
		if (op == OPUSH || op == BIN || op == UNA) {
			i++;
			token t = get_token(&bytecode[i], &i);
			print_token_inline(&t, buffer);
		}
		else if (op == BIND || op == WHERE) {
			i++;
			char* c = (char*)(bytecode + i);
			fprintf(buffer, "%s", c);
			i++; // for null term
		}
		else if (op == OREQ || op == JMP || op == PLIST) {
			i++;
			fprintf(buffer, "0x%X", bytecode[i]);
		}
		long int count = ftell(buffer) - oldChar;
		while (count++ < 20) {
			fprintf(buffer, " ");
		}
		fprintf(buffer, "| ");
		for (int j = start; j <= i; j++) {
			if (j != start) fprintf(buffer, " ");
			fprintf(buffer, "%02X", bytecode[j]);
		}
		fprintf(buffer, "\n");
	}
}

