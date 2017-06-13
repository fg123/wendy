#include "ast.h"
#include "global.h"
#include <stdbool.h>
#include <stdio.h>
#include "token.h"
#include "codegen.h"
#include "error.h"
#include <string.h>
#include "memory.h"
#include "execpath.h"

// Implementation of Wendy ByteCode Generator

static uint8_t* bytecode = 0;
static size_t capacity = 0;
static size_t size = 0;
static int global_loop_id = 0;

int verify_header(uint8_t* bytecode) {
	char* start = (char*)bytecode;
	if (strcmp(WENDY_VM_HEADER, start) == 0) {
		return strlen(WENDY_VM_HEADER) + 1;
	}
	else {
		error_general(GENERAL_INVALID_HEADER); 
	}
	return 0;
}

static void guarantee_size() {
	if (size + CODEGEN_PAD_SIZE	> capacity) {
		capacity *= 2;
		uint8_t* re = safe_realloc(bytecode, capacity * sizeof(uint8_t));
		bytecode = re;
	}
}

static void write_string(char* string) {
	for (int i = 0; string[i]; i++) {
		bytecode[size++] = string[i];
	}
	bytecode[size++] = 0;
	guarantee_size();
}

static void write_address(address a) {
	uint8_t* first = (void*)&a;
	for (int i = 0; i < sizeof(a); i++) {
		bytecode[size++] = first[i];
	}
	guarantee_size();
}

static void write_address_at(address a, int pos) {
	uint8_t* first = (void*)&a;
	for (int i = 0; i < sizeof(a); i++) {
		bytecode[pos++] = first[i];
	}
}

static void write_double_at(double a, int pos) {
	unsigned char * p = (void*)&a;
	for (int i = 0; i < sizeof(double); i++) {
		bytecode[pos++] = p[i];
	}

}

static void write_integer(int a) {
	write_address(a);
}

// writes the token to the bytestream, guaranteed to be a literal
static void write_token(token t) {
	bytecode[size++] = t.t_type;
	if (is_numeric(t)) {
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

static void write_opcode(opcode op) {
	bytecode[size++] = op;
	guarantee_size();
}

// Generates Evaluates lValue expr and stores in memory register

static void codegen_expr(void* expre);
static void codegen_statement(void* expre);
static void codegen_statement_list(void* expre);
static void codegen_lvalue_expr(expr* expression) {
	if (expression->type == E_LITERAL) {
		// Better be a identifier eh
		if (expression->op.lit_expr.t_type != IDENTIFIER) {
			error_lexer(expression->op.lit_expr.t_line,
				expression->op.lit_expr.t_col,
				CODEGEN_LVALUE_EXPECTED_IDENTIFIER);
		}
		write_opcode(WHERE);
		write_string(expression->op.lit_expr.t_data.string);
	}
	else if (expression->type == E_BINARY) {
		// Left side in memory reg
		codegen_lvalue_expr(expression->op.bin_expr.left);

		if (expression->op.bin_expr.operator.t_type == DOT) {
			if (expression->op.bin_expr.right->type != E_LITERAL) {
				error_lexer(expression->line, expression->col,
					CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL);
			}
			expression->op.bin_expr.right->op.lit_expr.t_type = MEMBER;
			write_opcode(OPUSH);
			write_token(expression->op.bin_expr.right->op.lit_expr);
			write_opcode(MEMPTR);	
		}	
		else if (expression->op.bin_expr.operator.t_type == LEFT_BRACK) {
			codegen_expr(expression->op.bin_expr.right);	
			write_opcode(NTHPTR);
		}
		else {
			error_lexer(expression->line, expression->col, 
					CODEGEN_INVALID_LVALUE_BINOP); 
		}
	}
	else {
		error_lexer(expression->line, expression->col, CODEGEN_INVALID_LVALUE);
	}
}

static void codegen_expr_list(void* expre) {
	expr_list* list = (expr_list*) expre;
	while (list) {
		codegen_expr(list->elem);
		list = list->next;
	}
}

static opcode tok_to_opcode(token t) {
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
			error_general(GENERAL_NOT_IMPLEMENTED, token_string[t.t_type]);
	}
	return 0;
}

static void codegen_statement(void* expre) {
	if (!expre) return;
	statement* state = (statement*) expre;
	write_opcode(SRC);
	write_integer(state->src_line);
	if (state->type == S_LET) {
		codegen_expr(state->op.let_statement.rvalue);
		// Request Memory
		write_opcode(RBW);
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
	else if (state->type == S_IMPORT) {
		if (state->op.import_statement.t_type == STRING) {
			// Already handled by the scanner class.
			return;
		}
		// Has to be identifier now.
		char* path = get_path();
		long length = 0;
		
		uint8_t* buffer;
		strcat(path, "wendy-lib/");
		strcat(path, state->op.import_statement.t_data.string);
		strcat(path, ".wc");
		//printf("Attempting to open: %s\n", path);
		FILE * f = fopen(path, "r");
		if (f) {
			fseek (f, 0, SEEK_END);
			length = ftell (f);
			fseek (f, 0, SEEK_SET);
			buffer = safe_malloc(length); // + 1 for null terminator
			fread (buffer, sizeof(uint8_t), length, f);
			write_opcode(BADR);
			int offset = size + sizeof(int) - strlen(WENDY_VM_HEADER) - 1;
			write_integer(offset);
			for (long i = verify_header(buffer); i < length; i++) {
				if (i == length - 1 && buffer[i] == HALT) break;
				bytecode[size++] = buffer[i];
				guarantee_size();
			}
			write_opcode(BADR);
			write_integer(-1 * offset);
			safe_free(buffer);
			fclose (f);
		}
		else {
			error_lexer(state->op.import_statement.t_line, 
						state->op.import_statement.t_col, 
						CODEGEN_REQ_FILE_READ_ERR);
		}
		safe_free(path);
	}
	else if (state->type == S_STRUCT) {
		int push_size = 2; // For Header and Name
		char* struct_name = state->op.struct_statement.name.t_data.string;
		
		// Push Header and Name
		write_opcode(OPUSH);
		int metaHeaderLoc = size;
		write_token(make_token(STRUCT_METADATA, make_data_num(1)));
		write_opcode(OPUSH);
		write_token(make_token(STRUCT_NAME, make_data_str(struct_name)));
		write_opcode(OPUSH);
		write_token(make_token(STRUCT_STATIC, make_data_str("init")));
		// Push Init Function
		codegen_expr(state->op.struct_statement.init_fn);

		push_size += 2;

		if (state->op.struct_statement.parent.t_type != EMPTY) {
			// Write Parent, but assert first
			write_opcode(WHERE);
			write_string(state->op.struct_statement.parent.t_data.string);
			write_opcode(ASSERT);
			bytecode[size++] = STRUCT;
			write_string(CODEGEN_PARENT_NOT_STRUCT);

			write_opcode(READ); // Read Parent Element In
			write_opcode(CHTYPE);  // Store Address of Struct Meta in Data Reg
			bytecode[size++] = STRUCT_PARENT;
			write_opcode(OPUSH);
			write_token(make_token(STRUCT_PARAM, make_data_str("base")));
			push_size += 2;
		}
	
		expr_list* curr = state->op.struct_statement.instance_members;
		while (curr) {
			expr* elem = curr->elem;
			if (elem->type != E_LITERAL 
				|| elem->op.lit_expr.t_type != IDENTIFIER) {
				error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
			}
			write_opcode(OPUSH);
			write_token(make_token(STRUCT_PARAM, elem->op.lit_expr.t_data));
			push_size++;
			curr = curr->next;
		}
		curr = state->op.struct_statement.static_members;
		while (curr) {
			expr* elem = curr->elem;
			if (elem->type != E_LITERAL 
				|| elem->op.lit_expr.t_type != IDENTIFIER) {
				error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
			}
			write_opcode(OPUSH);
			write_token(make_token(STRUCT_STATIC, elem->op.lit_expr.t_data));
			write_opcode(OPUSH);
			write_token(none_token());
			push_size += 2;
			curr = curr->next;
		}
		// Request Memory to Store MetaData
		write_opcode(OREQ);
		bytecode[size++] = push_size;
		write_opcode(PLIST);
		bytecode[size++] = push_size;
		// Now there's a List Token at the top of the stack.
		write_opcode(CHTYPE);
		bytecode[size++] = STRUCT;
		
		write_opcode(RBW);
		write_string(struct_name);
		write_double_at(push_size, metaHeaderLoc + 1);	
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
		write_opcode(OPUSH);
		write_token(make_token(NUMBER, make_data_num(0)));
		write_opcode(RBW);
		char loopIndexName[30];
		sprintf(loopIndexName, ":\")%d", global_loop_id++);
		write_string(loopIndexName);

		write_opcode(FRM); // Start Local Variable Frame OUTER
		if (state->op.loop_statement.index_var.t_type != EMPTY) {
			// User has a custom variable, also load that.
			write_opcode(OPUSH);
			write_token(make_token(NUMBER, make_data_num(0)));
			write_opcode(RBW);
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

static void codegen_statement_list(void* expre) {
	statement_list* list = (statement_list*) expre;
	while (list) {
		codegen_statement(list->elem);
		list = list->next;
	}	
}

static void codegen_expr(void* expre) {
	if (!expre) return;
	expr* expression = (expr*)expre;
	if (expression->type == E_LITERAL) {
		// Literal Expression, we push to the stack.
		write_opcode(OPUSH);
		write_token(expression->op.lit_expr);
	}
	else if (expression->type == E_BINARY) {
		if (expression->op.bin_expr.operator.t_type == DOT) {
			codegen_expr(expression->op.bin_expr.left);
			if (expression->op.bin_expr.right->type != E_LITERAL) {
				error_lexer(expression->line, expression->col,
					CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL);
			}
			expression->op.bin_expr.right->op.lit_expr.t_type = MEMBER;
			write_opcode(OPUSH);
			write_token(expression->op.bin_expr.right->op.lit_expr);
		}
		else {
			codegen_expr(expression->op.bin_expr.left);
			codegen_expr(expression->op.bin_expr.right);
		}
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
		codegen_expr(expression->op.assign_expr.rvalue);
		
		codegen_lvalue_expr(expression->op.assign_expr.lvalue);
		if (operator.t_type != EQUAL) {
			write_opcode(READ);
			write_opcode(RBIN);
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
		write_opcode(OPUSH);
		write_token(make_token(LIST_HEADER, make_data_num(count)));
		expr_list* param = expression->op.list_expr.contents;
		while (param) {
			codegen_expr(param->elem);
			param = param->next;
		}
		write_opcode(OREQ);
		// For the Header
		bytecode[size++] = count + 1;
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
			write_opcode(RBW);
			write_string(t.t_data.string);
			param = param->next;
		}
		if (expression->op.func_expr.body->type == S_EXPR) {
			codegen_expr(expression->op.func_expr.body->op.expr_statement);
			write_opcode(ORET);
		}
		else {
			codegen_statement(expression->op.func_expr.body);
			if (bytecode[size - 1] != ORET) {
				// Function has no explicit Return
				write_opcode(OPUSH);
				write_token(noneret_token());
				write_opcode(ORET);
			}
		}
		write_address_at(size, writeSizeLoc);
		write_opcode(OPUSH);
		write_token(make_token(ADDRESS, make_data_num(startAddr)));
		write_opcode(CLOSUR);
		write_opcode(OREQ);
		bytecode[size++] = 2;
		write_opcode(PLIST);
		bytecode[size++] = 2;
		write_opcode(CHTYPE);
		bytecode[size++] = FUNCTION;
	}
	else {
		printf("Traverse Expr: Unknown Type\n");
	}
}

uint8_t* generate_code(statement_list* _ast) {
	// reset
	capacity = CODEGEN_START_SIZE;
	bytecode = safe_malloc(capacity * sizeof(uint8_t));
	size = 0;
	global_loop_id = 0;
	write_string(WENDY_VM_HEADER);
	codegen_statement_list(_ast);
	write_opcode(HALT);
	return bytecode;
}

token get_token(uint8_t* bytecode, unsigned int* end) {
	token t;
	int start = *end;
	int i = 0;
	t.t_type = bytecode[i++];
	t.t_data.string[0] = 0;
	if (is_numeric(t)) {
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

void write_bytecode(uint8_t* bytecode, FILE* buffer) {
	fwrite(bytecode, sizeof(uint8_t), size, buffer);
}

void print_bytecode(uint8_t* bytecode, FILE* buffer) {
	fprintf(buffer, RED "WendyVM ByteCode Disassembly\n" GRN ".header\n");
	fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: ", &bytecode[0], 0);
	fprintf(buffer, YEL WENDY_VM_HEADER);
	fprintf(buffer, GRN "\n.code\n");
	int maxlen = 12;
	int baseaddr = 0;
	for (unsigned int i = verify_header(bytecode);; i++) {
		unsigned int start = i;
		fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: " RESET, &bytecode[i], i);
		opcode op = bytecode[i];
		long int oldChar = ftell(buffer);
		fprintf(buffer, YEL "%6s " RESET, opcode_string[op]);
		if (op == OPUSH || op == BIN || op == UNA || op == RBIN) {
			i++;
			token t = get_token(&bytecode[i], &i);
			if (t.t_type == STRING) {	
				fprintf(buffer, "%.*s ", maxlen, t.t_data.string);
				if (strlen(t.t_data.string) > maxlen) {
					fprintf(buffer, ">");
				}
			}
			else {
				print_token_inline(&t, buffer);
			}
		}
		else if (op == BIND || op == WHERE || op == RBW) {
			i++;
			char* c = (char*)(bytecode + i);
			fprintf(buffer, "%.*s ", maxlen, c);
			if (strlen(c) > maxlen) {
				fprintf(buffer, ">");
			}

			i += strlen(c);
		}
		else if (op == CHTYPE) {
			i++;
			fprintf(buffer, "%s", token_string[bytecode[i]]);
		}
		else if (op == OREQ || op == PLIST) {
			i++;
			fprintf(buffer, "0x%X", bytecode[i]);
		}
		else if (op == BADR) {
			void* loc = &bytecode[i + 1];
			i += sizeof(int);
			int offset = *((int*)loc);
			if (offset < 0) {
				fprintf(buffer, "-0x%X", -offset);
			}
			else {
				fprintf(buffer, "0x%X", offset);
			}
			baseaddr += offset;
		}
		else if (op == SRC) {
			void* loc = &bytecode[i + 1];
			i += sizeof(address);
			fprintf(buffer, "0x%X", *((address*)loc));
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
			fprintf(buffer, "%.*s ", maxlen, c);
			if (strlen(c) > maxlen) {
				fprintf(buffer, ">");
			}
			i += strlen(c);
		}
		else if (op == LBIND) {
			i++;
			char* c = (char*)(bytecode + i);
			fprintf(buffer, "%.*s ", maxlen, c);
			i += strlen(c);
			i++;
			c = (char*)(bytecode + i);
			fprintf(buffer, "%.*s ", maxlen, c);
			i += strlen(c);
		}
		else if (op == ASSERT) {
			i++;
			fprintf(buffer, "%s <error>", token_string[bytecode[i]]);
			i++;
			char* c = (char*)(bytecode + i);
			i += strlen(c);
		}
		long int count = ftell(buffer) - oldChar;
		while (count++ < 30) {
			fprintf(buffer, " ");
		}
		fprintf(buffer, CYN "| ");
		for (int j = start; j <= i; j++) {
			if (j != start) fprintf(buffer, " ");
			fprintf(buffer, "%02X", bytecode[j]);
		}
		fprintf(buffer, "\n" RESET);
		if (op == HALT) {
			break;
		}
	}
}

