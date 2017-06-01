#include "ast.h"
#include "macros.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "token.h"
#include "codegen.h"
#include "error.h"
#include <string.h>
#include "memory.h"

// Implementation of Wendy ByteCode Generator

static uint8_t* bytecode = 0;
static size_t capacity = 0;
static size_t size = 0;
static int global_loop_id = 0;

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

void write_address(address a) {
	uint8_t* first = (void*)&a;
	for (int i = 0; i < sizeof(a); i++) {
		bytecode[size++] = first[i];
	}
	guarantee_size();
}

void write_address_at(address a, int pos) {
	uint8_t* first = (void*)&a;
	for (int i = 0; i < sizeof(a); i++) {
		bytecode[pos++] = first[i];
	}
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

void codegen_expr(void* expre);
void codegen_statement(void* expre);
void codegen_statement_list(void* expre);
void codegen_lvalue_expr(expr* expression) {
	if (expression->type == E_LITERAL) {
		// Better be a identifier eh
		if (expression->op.lit_expr.t_type != IDENTIFIER) {
			error(expression->op.lit_expr.t_line, LVALUE_EXPECTED_IDENTIFIER);
		}
		write_opcode(WHERE);
		write_string(expression->op.lit_expr.t_data.string);
	}
	else if (expression->type == E_BINARY) {
		// Left side in memory reg
		codegen_lvalue_expr(expression->op.bin_expr.left);
		// Write Right side as Usual
		codegen_expr(expression->op.bin_expr.right);	

		if (expression->op.bin_expr.operator.t_type == DOT) {
			write_opcode(MEMPTR);	
		}	
		else if (expression->op.bin_expr.operator.t_type == LEFT_BRACK) {
			write_opcode(NTHPTR);
		}
		else {
			error(0, INVALID_LVALUE); 
		}
	}
	else {
		error(0, INVALID_LVALUE);
	}
}
void codegen_expr_list(void* expre) {
	expr_list* list = (expr_list*) expre;
	while (list) {
		codegen_expr(list->elem);
		list = list->next;
	}
}
opcode tok_to_opcode(token t) {
	switch(t.t_type) {
		case INC:
			return OINC;
		case DEC:
			return ODEC;
		case INPUT:
			return IN;
		case EXPLODE:
		case REQ:
		default:
			error(0, "TO BE IMPLEMENTED");
	}
	return 0;
}

void codegen_statement(void* expre) {
	if (!expre) return;
	statement* state = (statement*) expre;
	if (state->type == S_LET) {
		codegen_expr(state->op.let_statement.rvalue);
		// Request Memory
		write_opcode(OREQ);
		bytecode[size++] = 1;
		write_opcode(WRITE);
		write_opcode(BIND);
		write_string(state->op.let_statement.lvalue.t_data.string);			
	}
	else if (state->type == S_OPERATION) {
		if (state->op.operation_statement.operator.t_type == RET) {
			codegen_expr(state->op.operation_statement.operand);
			write_opcode(ORET);
		}
		else if (state->op.operation_statement.operator.t_type == AT) {
			codegen_expr(state->op.operation_statement.operand);
			write_opcode(OUTL);
		}
		else {
			codegen_lvalue_expr(state->op.operation_statement.operand);
			write_opcode(tok_to_opcode(state->op.operation_statement.operator));
		}
	}
	else if (state->type == S_EXPR) {
		codegen_expr(state->op.expr_statement);
		// Only output if it's not an assignment statement.
		if (state->op.expr_statement &&
			state->op.expr_statement->type != E_ASSIGN) write_opcode(OUT);
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
		codegen_expr(state->op.if_statement.condition);
		write_opcode(JIF);
		int falseJumpLoc = size;
		size += sizeof(address);

		codegen_statement(state->op.if_statement.statement_true);
		write_opcode(JMP);
		int doneJumpLoc = size;
		size += sizeof(address);
		write_address_at(size, falseJumpLoc);
		codegen_statement(state->op.if_statement.statement_false);
		write_address_at(size, doneJumpLoc);
	}
	else if (state->type == S_LOOP) {
		// Setup Loop Index
		write_opcode(OREQ);
		bytecode[size++] = 1;
		write_opcode(OPUSH);
		write_token(make_token(NUMBER, make_data_num(0)));
		write_opcode(WRITE);
		write_opcode(BIND);
		char loopIndexName[30];
		sprintf(loopIndexName, "~O.o~%d", global_loop_id++);
		write_string(loopIndexName);

		write_opcode(FRM); // Start Local Variable Frame OUTER
		if (state->op.loop_statement.index_var.t_type != EMPTY) {
			// User has a custom variable, also load that.
			write_opcode(OREQ);
			bytecode[size++] = 1;
			write_opcode(OPUSH);
			write_token(make_token(NUMBER, make_data_num(0)));
			write_opcode(WRITE);
			write_opcode(BIND);
			write_string(state->op.loop_statement.index_var.t_data.string);
		}

		// Start of Loop, Push Condition to Stack
		int loop_start_addr = size;
		codegen_expr(state->op.loop_statement.condition);
		
		// Check Condition and Jump if Needed
		write_opcode(LJMP);
		int loop_skip_loc = size;
		size += sizeof(address);
		write_string(loopIndexName);

		write_opcode(FRM); // Start Local Variable Frame	
			
		// Write Custom Var and Bind
		if (state->op.loop_statement.index_var.t_type != EMPTY) {
			write_opcode(LBIND);
			write_string(state->op.loop_statement.index_var.t_data.string);
			write_string(loopIndexName);
		}
		else {
			// LJMP Only pops if it jumps, otherwise leaves it on the stack,
			//   LBIND will pop it, but if we dont LBIND we need to POP
			write_opcode(OPOP); 
		}

		codegen_statement(state->op.loop_statement.statement_true);
		write_opcode(END);
		write_opcode(WHERE);
		write_string(loopIndexName);
		write_opcode(OINC);
		if (state->op.loop_statement.index_var.t_type != EMPTY) {
			write_opcode(READ);
			write_opcode(WHERE);
			write_string(state->op.loop_statement.index_var.t_data.string);
			write_opcode(WRITE);
		}
		write_opcode(JMP);
		write_address(loop_start_addr);

		// Write End of Loop
		write_address_at(size, loop_skip_loc);
		write_opcode(END);
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
	else if (expression->type == E_ASSIGN) {
		token operator = expression->op.assign_expr.operator;
		switch(operator.t_type) {
			case ASSIGN_PLUS:
				operator.t_type = PLUS;
				strcpy(operator.t_data.string, "+");
				break;
			case ASSIGN_MINUS:
				operator.t_type = MINUS;
				strcpy(operator.t_data.string, "-");
				break;
			case ASSIGN_STAR:
				operator.t_type = STAR;
				strcpy(operator.t_data.string, "*");
				break;
			case ASSIGN_SLASH:
				operator.t_type = SLASH;
				strcpy(operator.t_data.string, "/");
				break;
			case ASSIGN_INTSLASH:
				operator.t_type = INTSLASH;
				strcpy(operator.t_data.string, "\\");
				break;
			default: break;
		}
		codegen_lvalue_expr(expression->op.assign_expr.lvalue);
		if (operator.t_type != EQUAL) {
			write_opcode(READ);
		}
		codegen_expr(expression->op.assign_expr.rvalue);

		// Expression stored on top of stack.
		if (operator.t_type != EQUAL) {
			write_opcode(BIN);
			write_token(operator);
		}
		// Memory Register should still be where lvalue is	
		write_opcode(WRITE);
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
		write_opcode(READ);
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
		int writeSizeLoc = size;
		size += sizeof(address);
		int startAddr = size;
		// Parameters are reversed from AST Generation
		expr_list* param = expression->op.func_expr.parameters;
		while (param) {
			token t = param->elem->op.lit_expr;
			write_opcode(OREQ);
			bytecode[size++] = 1;
			write_opcode(BIND);
			write_string(t.t_data.string);
			write_opcode(WRITE);
			param = param->next;
		}
		codegen_statement_list(expression->op.func_expr.body);
		if (bytecode[size - 1] != ORET) {
			// Function has no explicit Return
			write_opcode(OPUSH);
			write_token(noneret_token());
			write_opcode(ORET);
		}
		write_address_at(size, writeSizeLoc);
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
	global_loop_id = 0;
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
			i += strlen(c);
		}
		else if (op == OREQ || op == PLIST) {
			i++;
			fprintf(buffer, "0x%X", bytecode[i]);
		}
		else if (op == JMP || op == JIF) {
			void* loc = &bytecode[i + 1];
			i += sizeof(address);
			fprintf(buffer, "0x%X", *((address*)loc));
		}
		else if (op == LJMP) {
			void* loc = &bytecode[i + 1];
			i += sizeof(address);
			fprintf(buffer, "0x%X ", *((address*)loc));
			i++;
			char* c = (char*)(bytecode + i);
			fprintf(buffer, "%s", c);
			i += strlen(c);
		}
		else if (op == LBIND) {
			i++;
			char* c = (char*)(bytecode + i);
			fprintf(buffer, "%s ", c);
			i += strlen(c);
			i++;
			c = (char*)(bytecode + i);
			fprintf(buffer, "%s", c);
			i += strlen(c);
		}
		long int count = ftell(buffer) - oldChar;
		while (count++ < 30) {
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

