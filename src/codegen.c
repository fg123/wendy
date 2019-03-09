#include "codegen.h"
#include "ast.h"
#include "global.h"
#include "error.h"
#include "memory.h"
#include "execpath.h"
#include "operators.h"
#include "source.h"
#include "data.h"
#include "imports.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define write_byte(op) do { bytecode[size++] = op; } while(0)

// Implementation of Wendy ByteCode Generator
const char* opcode_string[] = {
	OPCODE_STRING,
	0 // Sentinal value used when traversing through this array; acts as a NULL
};

static uint8_t* bytecode = 0;
static size_t capacity = 0;
static size_t size = 0;
static size_t global_loop_id = 0;

int verify_header(uint8_t* bytecode) {
	char* start = (char*)bytecode;
	if (streq(WENDY_VM_HEADER, start)) {
		return strlen(WENDY_VM_HEADER) + 1;
	}
	else {
		error_general(GENERAL_INVALID_HEADER);
	}
	return 0;
}

static void guarantee_size(size_t desired_additional) {
	if (size + desired_additional + CODEGEN_PAD_SIZE > capacity) {
		capacity += desired_additional + CODEGEN_PAD_SIZE;
		uint8_t* re = safe_realloc(bytecode, capacity * sizeof(uint8_t));
		bytecode = re;
	}
}

static void write_string(char* string) {
	if (!string) {
		return;
	}
	guarantee_size(strlen(string) + 1);
	for (size_t i = 0; string[i]; i++) {
		write_byte(string[i]);
	}
	write_byte(0);
}

static void write_address(address a) {
	guarantee_size(sizeof(address));
	size_t pos = size;
	if (!is_big_endian) pos += sizeof(a);
	size += sizeof(a);
	uint8_t* first = (void*)&a;
	for (size_t i = 0; i < sizeof(address); i++) {
		bytecode[is_big_endian ? pos++ : --pos] = first[i];
	}
}

static void write_address_at(address a, int pos) {
	if (!is_big_endian) pos += sizeof(address);
	uint8_t* first = (void*)&a;
	for (size_t i = 0; i < sizeof(address); i++) {
		bytecode[is_big_endian ? pos++ : --pos] = first[i];
	}
}

static void write_double_at(double a, int pos) {
	if (!is_big_endian) pos += sizeof(a);
	uint8_t* p = (void*)&a;
	for (size_t i = 0; i < sizeof(double); i++) {
		bytecode[is_big_endian ? pos++ : --pos] = p[i];
	}
}

static void write_double(double a) {
	guarantee_size(sizeof(double));
	size_t pos = size;
	if (!is_big_endian) pos += sizeof(a);
	size += sizeof(double);
	uint8_t* p = (void*)&a;
	for (size_t i = 0; i < sizeof(double); i++) {
		bytecode[is_big_endian ? pos++ : --pos] = p[i];
	}
}

static void write_integer(int a) {
	write_address(a);
}

// writes data to stream, destroys data
static void write_data(data t) {
	write_byte(t.type);
	if (is_numeric(t)) {
		// Writing a double
		write_double(t.value.number);
	}
	else {
		write_string(t.value.string);
	}
	destroy_data(&t);
}

static inline void write_opcode(opcode op) {
	guarantee_size(1);
	write_byte(op);
}

static void codegen_expr(void* expre);
static void codegen_statement(void* expre);
static void codegen_statement_list(void* expre);

static void codegen_lvalue_expr(expr* expression) {
	if (expression->type == E_LITERAL) {
		// Better be a identifier eh
		if (expression->op.lit_expr.type != D_IDENTIFIER) {
			error_lexer(expression->line, expression->col,
				CODEGEN_LVALUE_EXPECTED_IDENTIFIER);
			return;
		}
		write_opcode(OP_WHERE);
		write_string(expression->op.lit_expr.value.string);
	}
	else if (expression->type == E_BINARY) {
		// Left side in memory reg
		codegen_expr(expression->op.bin_expr.left);

		if (expression->op.bin_expr.operator == O_MEMBER) {
			if (expression->op.bin_expr.right->type != E_LITERAL) {
				error_lexer(expression->line, expression->col,
					CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL);
				return;
			}
			//expression->op.bin_expr.right->op.lit_expr.t_type = T_MEMBER;
			write_opcode(OP_MEMPTR);
			write_string(expression->op.bin_expr.right->op.lit_expr.value.string);
		}
		else if (expression->op.bin_expr.operator == O_SUBSCRIPT) {
			codegen_expr(expression->op.bin_expr.right);
			write_opcode(OP_NTHPTR);
		}
		else {
			error_lexer(expression->line, expression->col,
					CODEGEN_INVALID_LVALUE_BINOP);
		}
	}
	else if (expression->type == E_CALL) {
		codegen_expr(expression);
	}
	else {
		error_lexer(expression->line, expression->col, CODEGEN_INVALID_LVALUE);
	}
}

static inline void codegen_end_marker(void) {
	write_opcode(OP_PUSH);
	write_data(make_data(D_END_OF_ARGUMENTS,
		data_value_num(0)));
}

static void codegen_expr_list_for_call_named(expr_list* list) {
	if (!list) {
		codegen_end_marker();
		return;
	}
	if (list->elem->type != E_ASSIGN) {
		error_lexer(list->elem->line, list->elem->col,
			CODEGEN_NAMED_ARGUMENT_MUST_COME_AFTER_POSITIONAL);
		return;
	}
	codegen_expr_list_for_call_named(list->next);
	// Named Argument
	expr* assign_expr = list->elem;
	if (assign_expr->op.assign_expr.lvalue->type != E_LITERAL ||
		assign_expr->op.assign_expr.lvalue->op.lit_expr.type != D_IDENTIFIER) {
		error_lexer(assign_expr->line,
					assign_expr->col,
					CODEGEN_NAMED_ARGUMENT_NOT_LITERAL);
		return;
	}

	codegen_expr(assign_expr->op.assign_expr.rvalue);
	write_opcode(OP_PUSH);
	write_data(make_data(D_NAMED_ARGUMENT_NAME,
		data_value_str(assign_expr->op.assign_expr.lvalue->op.lit_expr.value.string)));
}

static void codegen_expr_list_for_call(expr_list* list) {
	// Let the recursion handle the reversing of the generation.
	if (!list) {
		// Write end marker first.
		codegen_end_marker();
		return;
	}
	if (list->elem->type != E_ASSIGN) {
		// Simple Case
		codegen_expr_list_for_call(list->next);
		codegen_expr(list->elem);
		return;
	}
	codegen_expr_list_for_call_named(list);
}

static void codegen_statement(void* expre) {
	if (!expre) return;
	statement* state = (statement*) expre;

	if (!get_settings_flag(SETTINGS_COMPILE)) {
		write_opcode(OP_SRC);
		write_integer(state->src_line);
	}
	if (state->type == S_LET) {
		codegen_expr(state->op.let_statement.rvalue);
		// Request Memory
		write_opcode(OP_DECL);
		write_string(state->op.let_statement.lvalue);
		write_opcode(OP_WRITE);
	}
	else if (state->type == S_OPERATION) {
		if (state->op.operation_statement.operator == OP_RET) {
			codegen_expr(state->op.operation_statement.operand);
		}
		else if (state->op.operation_statement.operator == OP_OUTL) {
			codegen_expr(state->op.operation_statement.operand);
		}
		else {
			codegen_lvalue_expr(state->op.operation_statement.operand);
		}
		write_opcode(state->op.operation_statement.operator);
	}
	else if (state->type == S_EXPR) {
		codegen_expr(state->op.expr_statement);
		// Only output if it's not an assignment statement.
		if (state->op.expr_statement &&
			state->op.expr_statement->type != E_ASSIGN) write_opcode(OP_OUT);
	}
	else if (state->type == S_BLOCK) {
		if (state->op.block_statement) {
			write_opcode(OP_FRM);
			codegen_statement_list(state->op.block_statement);
			if (bytecode[size - 1] != OP_RET) {
				/* Don't need to end the block if we immediately RET */
				write_opcode(OP_END);
			}
		}
	}
	else if (state->type == S_IMPORT) {
		if (!state->op.import_statement) {
			// Already handled by the scanner class.
			return;
		}
		char* library_name = state->op.import_statement;
		// Has to be identifier now.
		if (!has_already_imported_library(library_name)) {
			add_imported_library(library_name);
			write_opcode(OP_IMPORT);
			write_string(library_name);
			int jumpLoc = size;
			size += sizeof(address);

			// Could either be in local directory or in standard
			// library location. Local directory prevails.
			static char *extension = ".wc";
			char *local_path = safe_malloc(strlen(library_name) +
							   strlen(extension) + 1);
			local_path[0] = 0;
			strcat(local_path, library_name);
			strcat(local_path, extension);
			FILE *f = fopen(local_path, "r");
			safe_free(local_path);
			if (f) {
				goto file_found;
			}
			// Not found, try standard library.
			char* path = get_path();
			long length = 0;
			uint8_t* buffer;
			strcat(path, "wendy-lib/");
			strcat(path, library_name);
			strcat(path, extension);
			f = fopen(path, "r");
			safe_free(path);
			if (f) {
			file_found:
				fseek (f, 0, SEEK_END);
				length = ftell(f);
				fseek (f, 0, SEEK_SET);
				buffer = safe_malloc(length);
				fread (buffer, sizeof(uint8_t), length, f);
				int offset = size - strlen(WENDY_VM_HEADER) - 1;
				offset_addresses(buffer, length, offset);
				guarantee_size(length);
				for (long i = verify_header(buffer); i < length; i++) {
					if (i == length - 1 && buffer[i] == OP_HALT) break;
					write_byte(buffer[i]);
				}
				safe_free(buffer);
				fclose (f);
			}
			else {
				error_lexer(state->src_line, 0,
							CODEGEN_REQ_FILE_READ_ERR);
			}
			write_address_at(size, jumpLoc);
		}
	}
	else if (state->type == S_STRUCT) {
		int push_size = 2; // For Header and Name
		char* struct_name = state->op.struct_statement.name;

		// Push Header and Name
		write_opcode(OP_PUSH);
		int metaHeaderLoc = size;
		write_data(make_data(D_STRUCT_HEADER, data_value_num(1)));
		write_opcode(OP_PUSH);
		write_data(make_data(D_STRUCT_NAME, data_value_str(struct_name)));
		write_opcode(OP_PUSH);
		write_data(make_data(D_STRUCT_SHARED, data_value_str("init")));
		// Push Init Function
		codegen_expr(state->op.struct_statement.init_fn);

		push_size += 2;

		expr_list* curr = state->op.struct_statement.instance_members;
		while (curr) {
			expr* elem = curr->elem;
			if (elem->type != E_LITERAL
				|| elem->op.lit_expr.type != D_IDENTIFIER) {
				error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
			}
			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_PARAM,
				data_value_str(elem->op.lit_expr.value.string)));
			push_size++;
			curr = curr->next;
		}
		curr = state->op.struct_statement.static_members;
		while (curr) {
			expr* elem = curr->elem;
			if (elem->type != E_LITERAL
				|| elem->op.lit_expr.type != D_IDENTIFIER) {
				error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
			}
			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_SHARED,
				data_value_str(elem->op.lit_expr.value.string)));
			write_opcode(OP_PUSH);
			write_data(none_data());
			push_size += 2;
			curr = curr->next;
		}
		// Request Memory to Store MetaData
		write_opcode(OP_MKREF);
		write_byte(D_STRUCT);
		write_integer(push_size);

		write_opcode(OP_DECL);
		write_string(struct_name);
		write_opcode(OP_WRITE);
		write_double_at(push_size, metaHeaderLoc + 1);
	}
	else if (state->type == S_IF) {
		codegen_expr(state->op.if_statement.condition);
		write_opcode(OP_JIF);
		int falseJumpLoc = size;
		size += sizeof(address);

		write_opcode(OP_FRM);
		codegen_statement(state->op.if_statement.statement_true);
		write_opcode(OP_END);

		write_opcode(OP_JMP);
		int doneJumpLoc = size;
		size += sizeof(address);
		write_address_at(size, falseJumpLoc);

		write_opcode(OP_FRM);
		codegen_statement(state->op.if_statement.statement_false);
		write_opcode(OP_END);

		write_address_at(size, doneJumpLoc);
	}
	else if (state->type == S_LOOP) {
		if (!state->op.loop_statement.statement_true) {
			// Don't generate if empty loop body
			return;
		}
		write_opcode(OP_FRM);

		bool is_iterating_loop = state->op.loop_statement.index_var;

		char loop_index_name[30];
		char loop_container_name[30];
		char loop_size_name[30];

		if (is_iterating_loop) {
			size_t loop_id = global_loop_id++;
			sprintf(loop_index_name, LOOP_COUNTER_PREFIX "index_%zd", loop_id);
			sprintf(loop_container_name, LOOP_COUNTER_PREFIX "container_%zd", loop_id);
			sprintf(loop_size_name, LOOP_COUNTER_PREFIX "size_%zd", loop_id);

			// index = 0
			write_opcode(OP_PUSH);
			write_data(make_data(D_NUMBER, data_value_num(0)));
			write_opcode(OP_DECL);
			write_string(loop_index_name);
			write_opcode(OP_WRITE);

			// container = <expr>
			codegen_expr(state->op.loop_statement.condition);
			write_opcode(OP_DECL);
			write_string(loop_container_name);
			write_opcode(OP_WRITE);

			// size = container.size
			write_opcode(OP_PUSH);
			write_data(make_data(D_MEMBER_IDENTIFIER, data_value_str("size")));
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(loop_container_name)));
			write_opcode(OP_BIN);
			write_byte(O_MEMBER);
			write_opcode(OP_DECL);
			write_string(loop_size_name);
			write_opcode(OP_WRITE);

			// <ident> = none
			write_opcode(OP_PUSH);
			write_data(none_data());
			write_opcode(OP_DECL);
			write_string(state->op.loop_statement.index_var);
			write_opcode(OP_WRITE);
		}

		address loop_start_addr = size;
		if (is_iterating_loop) {
			// internalCounter < size
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(loop_size_name)));
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(loop_index_name)));
			write_opcode(OP_BIN);
			write_byte(O_LT);
		}
		else {
			codegen_expr(state->op.loop_statement.condition);
		}
		write_opcode(OP_JIF);
		int loop_skip_loc = size;
		size += sizeof(address);
		if (is_iterating_loop) {
			// <ident> = container[internalCounter]
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(loop_index_name)));
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(loop_container_name)));
			write_opcode(OP_BIN);
			write_byte(O_SUBSCRIPT);
			write_opcode(OP_WHERE);
			write_string(state->op.loop_statement.index_var);
			write_opcode(OP_WRITE);
		}
		codegen_statement(state->op.loop_statement.statement_true);
		if (is_iterating_loop) {
			write_opcode(OP_WHERE);
			write_string(loop_index_name);
			write_opcode(OP_INC);
		}
		write_opcode(OP_JMP);
		write_address(loop_start_addr);
		write_address_at(size, loop_skip_loc);

		write_opcode(OP_END);
	}
}

static void codegen_statement_list(void* expre) {
	statement_list* list = (statement_list*) expre;
	while (list) {
		codegen_statement(list->elem);
		if (bytecode[size - 1] == OP_RET) {
			/* Stop generating if last statement was a return */
			break;
		}
		list = list->next;
	}
}

static void codegen_expr(void* expre) {
	if (!expre) return;
	expr* expression = (expr*)expre;
	if (expression->type == E_LITERAL) {
		// Literal Expression, we push to the stack.
		write_opcode(OP_PUSH);
		write_data(copy_data(expression->op.lit_expr));
	}
	else if (expression->type == E_BINARY) {
		if (expression->op.bin_expr.operator == O_MOD_EQUAL) {
			/* Special operator just for Dhruvit, first we calculate the remainder */
			codegen_expr(expression->op.bin_expr.right);
			codegen_expr(expression->op.bin_expr.left);
			write_opcode(OP_BIN);
			write_byte(O_REM);

			/* Then we simulate a div_equals operation */
			codegen_expr(expression->op.bin_expr.right);
			codegen_expr(expression->op.bin_expr.left);
			write_opcode(OP_BIN);
			write_byte(O_IDIV);

			codegen_lvalue_expr(expression->op.bin_expr.left);
			write_opcode(OP_WRITE);
			/* Skip the default OP_BIN */
			return;
		}
		else if (expression->op.bin_expr.operator == O_MEMBER) {
			if (expression->op.bin_expr.right->type != E_LITERAL) {
				error_lexer(expression->line, expression->col,
					CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL);
			}
			expression->op.bin_expr.right->op.lit_expr.type = D_MEMBER_IDENTIFIER;
			write_opcode(OP_PUSH);
			write_data(copy_data(
                expression->op.bin_expr.right->op.lit_expr));
			codegen_expr(expression->op.bin_expr.left);
		}
		else {
			codegen_expr(expression->op.bin_expr.right);
			codegen_expr(expression->op.bin_expr.left);
		}
		write_opcode(OP_BIN);
		write_byte(expression->op.bin_expr.operator);
	}
	else if (expression->type == E_IF) {
		codegen_expr(expression->op.if_expr.condition);
		write_opcode(OP_JIF);
		int falseJumpLoc = size;
		size += sizeof(address);
		codegen_expr(expression->op.if_expr.expr_true);
		write_opcode(OP_JMP);
		int doneJumpLoc = size;
		size += sizeof(address);
		write_address_at(size, falseJumpLoc);
		if (expression->op.if_expr.expr_false) {
			codegen_expr(expression->op.if_expr.expr_false);
		}
		else {
			write_opcode(OP_PUSH);
			write_data(none_data());
		}
		write_address_at(size, doneJumpLoc);
	}
	else if (expression->type == E_ASSIGN) {
        enum operator op = expression->op.assign_expr.operator;

		if (op != O_ASSIGN) {
			codegen_expr(expression->op.assign_expr.lvalue);
		}

		codegen_expr(expression->op.assign_expr.rvalue);

        // O_ASSIGN is the default =
		if (op != O_ASSIGN) {
			write_opcode(OP_BIN);
			write_byte(op);
		}

		codegen_lvalue_expr(expression->op.assign_expr.lvalue);
		write_opcode(OP_WRITE);
	}
	else if (expression->type == E_UNARY) {
		codegen_expr(expression->op.una_expr.operand);
		write_opcode(OP_UNA);
		write_byte(expression->op.una_expr.operator);
	}
	else if (expression->type == E_CALL) {
		codegen_expr_list_for_call(expression->op.call_expr.arguments);
		codegen_expr(expression->op.call_expr.function);
		write_opcode(OP_CALL);
	}
	else if (expression->type == E_LIST) {
		int count = expression->op.list_expr.length;
		write_opcode(OP_PUSH);
		write_data(make_data(D_LIST_HEADER, data_value_num(count)));
		expr_list* param = expression->op.list_expr.contents;
		while (param) {
			codegen_expr(param->elem);
			param = param->next;
		}
		write_opcode(OP_MKREF);
		write_byte(D_LIST);
		write_integer(count + 1);
	}
	else if (expression->type == E_FUNCTION) {
		write_opcode(OP_JMP);
		int writeSizeLoc = size;
		size += sizeof(address);
		int startAddr = size;

		/* Count parameters */
		int count = 0;
		expr_list* param = expression->op.func_expr.parameters;
		while (param) {
			param = param->next;
			count++;
		}
		char** param_names = safe_malloc(sizeof(*param_names) * count);

		if (expression->op.func_expr.is_native) {
			write_opcode(OP_NATIVE);
			write_integer(count);
			write_string(expression->op.func_expr.native_name);
			write_opcode(OP_RET);

			expr_list* param = expression->op.func_expr.parameters;
			int i = 0;
			while (param) {
				if (param->elem->type == E_LITERAL) {
					param_names[i++] = param->elem->op.lit_expr.value.string;
				}
				else {
					error_lexer(param->elem->line,
						param->elem->col,
						CODEGEN_UNEXPECTED_FUNCTION_PARAMETER);
				}
				param = param->next;
			}
		}
		else {
			expr_list* param = expression->op.func_expr.parameters;
			bool has_encountered_default = false;
			int i = 0;
			while (param) {
				if (param->elem->type == E_LITERAL) {
					if (has_encountered_default) {
						error_lexer(param->elem->line,
							param->elem->col,
							CODEGEN_FUNCTION_DEFAULT_VALUES_AT_END);
					}
					if (param->elem->op.lit_expr.type != D_IDENTIFIER) {
						error_lexer(param->elem->line,
							param->elem->col,
							CODEGEN_UNEXPECTED_FUNCTION_PARAMETER);
					}
					data t = param->elem->op.lit_expr;
					write_opcode(OP_DECL);
					write_string(t.value.string);
					write_opcode(OP_WRITE);
					param_names[i++] = t.value.string;
				}
				else if (param->elem->type == E_ASSIGN) {
					has_encountered_default = true;
					// Bind Default Value First
					codegen_expr(param->elem->op.assign_expr.rvalue);
					write_opcode(OP_DECL);
					// TODO: Check if assign expr is literal identifier.
					write_string(param->elem->op.assign_expr.lvalue->
						op.lit_expr.value.string);
					// If the top of the stack is marker, this is no-op.
					write_opcode(OP_WRITE);

					write_opcode(OP_WHERE);
					write_string(param->elem->op.assign_expr.lvalue->
						op.lit_expr.value.string);
					write_opcode(OP_WRITE);
					param_names[i++] = param->elem->op.assign_expr.lvalue->
						op.lit_expr.value.string;
				}
				else {
					error_lexer(param->elem->line,
						param->elem->col,
						CODEGEN_UNEXPECTED_FUNCTION_PARAMETER);
				}
				param = param->next;
			}
			// Process named arguments.
			write_opcode(OP_ARGCLN);
			if (expression->op.func_expr.body &&
				expression->op.func_expr.body->type == S_EXPR) {
				codegen_expr(expression->op.func_expr.body->op.expr_statement);
				write_opcode(OP_RET);
			}
			else {
				codegen_statement(expression->op.func_expr.body);
				if (bytecode[size - 1] != OP_RET) {
					// Function has no explicit Return
					write_opcode(OP_PUSH);
					write_data(noneret_data());
					write_opcode(OP_RET);
				}
			}
		}
		write_address_at(size, writeSizeLoc);
		write_opcode(OP_PUSH);
		write_data(make_data(D_INSTRUCTION_ADDRESS, data_value_num(startAddr)));
		write_opcode(OP_CLOSURE);
		write_opcode(OP_PUSH);
		write_data(make_data(D_STRING, data_value_str("self")));

		/* Make a list of parameters names */
		if (!get_settings_flag(SETTINGS_OPTIMIZE)) {
			write_opcode(OP_PUSH);
			write_data(make_data(D_LIST_HEADER, data_value_num(count)));
			for (int i = 0; i < count; i++) {
				write_opcode(OP_PUSH);
				write_data(make_data(D_STRING, data_value_str(param_names[i])));
			}
			write_opcode(OP_MKREF);
			write_byte(D_LIST);
			write_integer(count + 1);
		}
		else {
			write_opcode(OP_PUSH);
			write_data(none_data());
		}

		safe_free(param_names);
		write_opcode(OP_MKREF);
		write_byte(D_FUNCTION);
		write_integer(4);
	}
}

uint8_t* generate_code(statement_list* _ast, size_t* size_ptr) {
	capacity = CODEGEN_START_SIZE;
	bytecode = safe_malloc(capacity * sizeof(uint8_t));
	size = 0;
	if (!get_settings_flag(SETTINGS_REPL)) {
		write_string(WENDY_VM_HEADER);
	}
	free_imported_libraries_ll();
	codegen_statement_list(_ast);
	free_imported_libraries_ll();
	write_opcode(OP_HALT);
	*size_ptr = size;
	return bytecode;
}

// CANNOT FREE OR DESTROY THIS ONE!
data get_data(uint8_t* bytecode, unsigned int* end) {
	// Bytecode is already Offset!
	data t;
	int start = *end;
	int i = 0;
	t.type = bytecode[i++];
	if (is_numeric(t)) {
		if (!is_big_endian) i += sizeof(double);
		unsigned char * p = (void*)&t.value.number;
		for (size_t j = 0; j < sizeof(double); j++) {
			p[j] = bytecode[is_big_endian ? i++ : --i];
		}
		if (!is_big_endian) i += sizeof(double);
	}
	else {
		t.value.string = (char*)bytecode + i;
		i += strlen((char*)bytecode + i) + 1;
	}
	*end = start + i;
	return t;
}

void write_bytecode(uint8_t* bytecode, FILE* buffer) {
	fwrite(bytecode, sizeof(uint8_t), size, buffer);
}

char* get_string(uint8_t* bytecode, unsigned int* end) {
	*end += strlen((char*)bytecode) + 1;
	return (char *)bytecode;
}

address get_address(uint8_t* bytecode, unsigned int* end) {
	// Bytecode is already offset
	int i = 0;
	if (!is_big_endian) i += sizeof(address);
	address result = 0;
	unsigned char * p = (void*)&result;
	for (size_t j = 0; j < sizeof(address); j++) {
		p[j] = bytecode[is_big_endian ? i++ : --i];
	}
	*end += sizeof(address);
	return result;
}

void print_bytecode(uint8_t* bytecode, FILE* buffer) {
	fprintf(buffer, RED "WendyVM ByteCode Disassembly\n" GRN ".header\n");
	fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: ", &bytecode[0], 0);
	fprintf(buffer, YEL WENDY_VM_HEADER);
	fprintf(buffer, GRN "\n.code\n");
	int max_len = 12;
	int baseaddr = 0;
	UNUSED(baseaddr);
	unsigned int i = 0;
	if (!get_settings_flag(SETTINGS_REPL)) {
		i = verify_header(bytecode);
	}
	forever {
		unsigned int start = i;
		fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: " RESET, &bytecode[i], i);
		opcode op = bytecode[i++];
		unsigned int p = 0;
		int printSourceLine = -1;
		p += fprintf(buffer, YEL "%6s " RESET, opcode_string[op]);

		switch (op) {
			case OP_PUSH: {
				data t = get_data(&bytecode[i], &i);
				if (t.type == D_STRING) {
					p += fprintf(buffer, "%.*s ", max_len, t.value.string);
					if (strlen(t.value.string) > (size_t) max_len) {
						p += fprintf(buffer, ">");
					}
				}
				else {
					p += print_data_inline(&t, buffer);
				}
				break;
			}
			case OP_BIN:
			case OP_UNA: {
				operator o = bytecode[i++];
				p += fprintf(buffer, "%s", operator_string[o]);
				break;
			}
			case OP_DECL:
			case OP_WHERE:
			case OP_IMPORT:
			case OP_MEMPTR: {
				char* c = get_string(bytecode + i, &i);
				p += fprintf(buffer, "%.*s ", max_len, c);
				if (strlen(c) > (size_t) max_len) {
					p += fprintf(buffer, ">");
				}
				if (op == OP_IMPORT) {
					// i is at null term
					p += fprintf(buffer, "0x%X", get_address(bytecode + i, &i));
				}
				break;
			}
			case OP_MKREF: {
				data_type t = bytecode[i++];
				p += fprintf(buffer, "%s", data_string[t]);
				address a = get_address(bytecode + i, &i);
				p += fprintf(buffer, " %d", a);
				break;
			}
			case OP_SRC: {
				address a = get_address(bytecode + i, &i);
				p += fprintf(buffer, "%d", a);
				printSourceLine = a;
				break;
			}
			case OP_JMP:
			case OP_JIF: {
				p += fprintf(buffer, "0x%X", get_address(bytecode + i, &i));
				break;
			}
			case OP_NATIVE: {
				p += fprintf(buffer, "%d ", get_address(bytecode + i, &i));
				char* c = get_string(bytecode + i, &i);
				p += fprintf(buffer, "%.*s", max_len, c);
				break;
			}
			default: break;
		}
		while (p++ < 30) {
			fprintf(buffer, " ");
		}
		fprintf(buffer, CYN "| ");
		for (size_t j = start; j < i; j++) {
			if (j != start) fprintf(buffer, " ");
			fprintf(buffer, "%02X", bytecode[j]);
			if (j > start + 2) {
				fprintf(buffer, "... ");
				break;
			}
			else if (j == i) {
				fprintf(buffer, "    ");
				break;
			}
		}
		if (printSourceLine > 0 && !get_settings_flag(SETTINGS_REPL)) {
			printf(RESET " %s", get_source_line(printSourceLine));
		}
		fprintf(buffer, "\n" RESET);
		if (op == OP_HALT) {
			break;
		}
	}
}

static void write_address_at_buffer(address a, uint8_t* buffer, size_t loc) {
	if (!is_big_endian) loc += sizeof(address);
	unsigned char * p = (void*)&a;
	for (size_t i = 0; i < sizeof(address); i++) {
		buffer[is_big_endian ? loc++ : --loc] = p[i];
	}
}

static void write_data_at_buffer(data t, uint8_t* buffer, size_t loc) {
	buffer[loc++] = t.type;
	if (t.type == D_INSTRUCTION_ADDRESS) {
		if (!is_big_endian) loc += sizeof(double);
		unsigned char * p = (void*)&t.value.number;
		for (size_t i = 0; i < sizeof(double); i++) {
			buffer[is_big_endian ? loc++ : --loc] = p[i];
		}
	}
}

void offset_addresses(uint8_t* buffer, size_t length, int offset) {
	UNUSED(length);
	// TODO: This is a little sketchy because of the difference in
	// headers between REPL mode and not.
	unsigned int i = 0;
	if (streq(WENDY_VM_HEADER, (char*)buffer)) {
		i += strlen(WENDY_VM_HEADER) + 1;
	}
	forever {
		opcode op = buffer[i++];
		switch (op) {
			case OP_PUSH: {
				size_t tokLoc = i;
				data t = get_data(buffer + i, &i);
				if (t.type == D_INSTRUCTION_ADDRESS) {
					t.value.number += offset;
					write_data_at_buffer(t, buffer, tokLoc);
				}
				break;
			}
			case OP_DECL:
			case OP_WHERE:
			case OP_MEMPTR: {
				get_string(buffer + i, &i);
				break;
			}
			case OP_IMPORT: {
				get_string(buffer + i, &i);
				unsigned int bi = i;
				address loc = get_address(buffer + i, &i);
				loc += offset;
				write_address_at_buffer(loc, buffer, bi);
				break;
			}
			case OP_SRC: {
				get_address(buffer + i, &i);
				break;
			}
			case OP_JMP:
			case OP_JIF: {
				unsigned int bi = i;
				address loc = get_address(buffer + i, &i);
				loc += offset;
				write_address_at_buffer(loc, buffer, bi);
				break;
			}
			// else if (op == OP_LJMP) {
			// 	unsigned int bi = i;
			// 	address loc = get_address(buffer + i, &i);
			// 	loc += offset;
			// 	write_address_at_buffer(loc, buffer, bi);
			// 	get_string(buffer + i, &i);
			// }
			// else if (op == OP_LBIND) {
			// 	get_string(buffer + i, &i);
			// 	get_string(buffer + i, &i);
			// }
			case OP_NATIVE: {
				get_address(buffer + i, &i);
				get_string(buffer + i, &i);
				break;
			}
			case OP_MKREF: {
				i++; // for the type
				get_address(buffer + i, &i);
				break;
			}
			case OP_BIN:
			case OP_UNA: {
				i++;
				break;
			}
			case OP_HALT: {
				return;
			}
			default: break;
		}
	}
}
