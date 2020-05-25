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
#include "locals.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define write_byte(op) do { bytecode[size++] = op; } while(0)

// Implementation of Wendy ByteCode Generator
const char* opcode_string[] = {
	OPCODE_STRING,
	0 // Sentinal value used when traversing through this array; acts as a NULL
};

// For the implementation of break and continue, we keep track of nested loops
struct loop_context {
	int scope;
	address break_locations[256];
	size_t break_count;

	address continue_locations[256];
	size_t continue_count;

	struct loop_context *parent;
};

struct loop_context *current_loop_context = 0;
int scope_level = 0;

static uint8_t* bytecode = 0;
static size_t capacity = 0;
static size_t size = 0;
static size_t global_loop_id = 0;
static size_t global_member_call_id = 0;

int verify_header(uint8_t* bytecode, size_t length) {
	char* start = (char*)bytecode;
	if (length > strlen(WENDY_VM_HEADER) + 1 &&
		streq(WENDY_VM_HEADER, start)) {
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

static void write_address_at(address a, address pos) {
	if (!is_big_endian) pos += sizeof(address);
	uint8_t* first = (void*)&a;
	for (size_t i = 0; i < sizeof(address); i++) {
		bytecode[is_big_endian ? pos++ : --pos] = first[i];
	}
}

static void write_double_at(double a, address pos) {
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
static void write_data(struct data t) {
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

static inline void write_opcode(enum opcode op) {
	guarantee_size(1);
	write_byte(op);
}

static void make_scope(void) {
	if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
		write_opcode(OP_FRM);
	}
	scope_level += 1;
}

static void end_scope(void) {
	if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
		write_opcode(OP_END);
	}
	scope_level -= 1;
	if (scope_level < 0) {
		error_general("Scope level below 0! make_scope and end_scope not aligned!");
	}
}

static void codegen_expr(void* expre);
static void codegen_statement(void* expre);
static void codegen_statement_list(void* expre);

static bool is_member_identifier(struct expr *e) {
	return e->type == E_LITERAL &&
		(e->op.lit_expr.type == D_MEMBER_IDENTIFIER);
}

static void codegen_lvalue_expr(struct expr* expression) {
	if (expression->type == E_LITERAL) {
		// Better be identifier, or offsets
		if (expression->op.lit_expr.type == D_IDENTIFIER) {
			write_opcode(OP_WHERE);
			write_string(expression->op.lit_expr.value.string);
		}
		else if (expression->op.lit_expr.type == D_IDENTIFIER_LOCAL_OFFSET) {
			write_opcode(OP_LOFFSET);
			write_address(expression->op.lit_expr.value.number);
		}
		else if (expression->op.lit_expr.type == D_IDENTIFIER_GLOBAL_OFFSET) {
			write_opcode(OP_GOFFSET);
			write_address(expression->op.lit_expr.value.number);
		}
		else {
			error_lexer(expression->line, expression->col,
				CODEGEN_LVALUE_EXPECTED_IDENTIFIER,
				data_string[expression->op.lit_expr.type]);
		}
	}
	else if (expression->type == E_BINARY) {
		// Left side in memory reg
		codegen_expr(expression->op.bin_expr.left);

		if (expression->op.bin_expr.operator == O_MEMBER) {
			assert(is_member_identifier(expression->op.bin_expr.right));
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

static void codegen_expr_list_for_call_named(struct expr_list* list) {
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
	struct expr* assign_expr = list->elem;
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

static void codegen_expr_list_for_call(struct expr_list* list) {
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

static void assert_one(size_t size, size_t *ptr) {
	if (*ptr >= size) {
		error_general("Expected argument (1 more token) in inline bytecode!");
	}
}

static int is_data_type(struct token t) {
	if (t.t_type != T_IDENTIFIER) return -1;
	for (size_t j = 0; data_string[j]; j++) {
		if (streq(t.t_data.string, data_string[j])) {
			return j;
		}
	}
	return -1;
}

// _size because global size taken; oops
static bool codegen_one_instruction(struct token *tokens, size_t _size, size_t * ptr) {
	if (*ptr >= _size) return false;
	bool found = false;
	enum opcode op;
	for (size_t j = 0; opcode_string[j]; j++) {
		if (streq(tokens[*ptr].t_data.string, opcode_string[j])) {
			op = (enum opcode) j;
			write_opcode(op);
			found = true;
			break;
		}
	}
	if (!found) {
		error_general("Unrecognized Instruction %s", tokens[*ptr].t_data.string);
		return false;
	}
	*ptr += 1;
	switch (op) {
		case OP_PUSH: {
			assert_one(_size, ptr);
			struct token arg = tokens[(*ptr)++];
			int maybe_data_type = is_data_type(arg);
			if (maybe_data_type >= 0) {
				assert_one(_size, ptr);
				struct token arg2 = tokens[(*ptr)++];
				if (arg2.t_type == T_NUMBER) {
					write_data(make_data((enum data_type) maybe_data_type,
						data_value_num(arg2.t_data.number)));
				}
				else if (arg2.t_type == T_STRING) {
					write_data(make_data((enum data_type) maybe_data_type,
						data_value_str(arg2.t_data.string)));
				}
				else {
					error_general("Invalid second arg to PUSH");
				}
			}
			else if (arg.t_type == T_NUMBER || arg.t_type == T_STRING ||
				arg.t_type == T_IDENTIFIER) {
				write_data(literal_to_data(arg));
			}
			else {
				error_general("Invalid arg(s) to PUSH");
			}
			break;
		}
		case OP_BIN:
		case OP_UNA: {
			assert_one(_size, ptr);
			struct token op_t = tokens[(*ptr)++];
			if (op == OP_BIN) {
				enum operator op_e = token_operator_binary(op_t);
				write_byte(op_e);
			}
			else {
				enum operator op_e = token_operator_unary(op_t);
				write_byte(op_e);
			}
			break;
		}
		case OP_CALL:
		case OP_RET:
			// NO ARGS
			break;
		case OP_DECL: {
			assert_one(_size, ptr);
			struct token arg = tokens[(*ptr)++];
			if (arg.t_type == T_IDENTIFIER) {
				write_string(arg.t_data.string);
			}
			else {
				error_general("Invalid args to DECL");
			}
			break;
		}
		case OP_WRITE:
		case OP_IN:
		case OP_OUT:
		case OP_OUTL:
			// NO ARGS
			break;
		case OP_GOFFSET:
		case OP_LOFFSET:
		case OP_JMP:
		case OP_JIF:
		case OP_SRC: {
			assert_one(_size, ptr);
			struct token arg = tokens[(*ptr)++];
			if (arg.t_type == T_NUMBER) {
				address a = (address) arg.t_data.number;
				write_address(a);
			}
			else {
				error_general("Invalid args to JMP or JIF");
			}
			break;
		}
		case OP_FRM:
		case OP_END:
			// NO ARGS
			break;
		case OP_HALT:
			break;
		case OP_NATIVE: {
			assert_one(_size, ptr);
			struct token arg = tokens[(*ptr)++];
			if (arg.t_type == T_NUMBER) {
				write_address((address) arg.t_data.number);
				assert_one(_size, ptr);
				struct token arg2 = tokens[(*ptr)++];
				if (arg2.t_type == T_IDENTIFIER) {
					write_string(arg2.t_data.string);
				}
				else {
					error_general("Invalid arg2 for NATIVE");
				}
			}
			else {
				error_general("Invalid args for NATIVE");
			}
			break;
		}
		case OP_WHERE:
		case OP_IMPORT: {
			assert_one(_size, ptr);
			struct token arg = tokens[(*ptr)++];
			if (arg.t_type == T_IDENTIFIER) {
				write_string(arg.t_data.string);
			}
			else {
				error_general("Invalid args for IMPORT");
			}
			break;
		}
		case OP_ARGCLN:
		case OP_CLOSURE:
			// NO ARGS
			break;
		case OP_MKREF: {
			assert_one(_size, ptr);
			struct token arg = tokens[(*ptr)++];
			int maybe_data_type = is_data_type(arg);
			if (maybe_data_type >= 0) {
				write_byte(maybe_data_type);
				assert_one(_size, ptr);
				struct token arg2 = tokens[(*ptr)++];
				if (arg2.t_type == T_NUMBER) {
					write_double(arg2.t_data.number);
				}
				else {
					error_general("Invalid arg2 for MKREF");
				}
			}
			else {
				error_general("Invalid arg for MKREF");
			}
			break;
		}
		case OP_NTHPTR:
		case OP_MEMPTR:
		case OP_INC:
		case OP_DEC:
			break;
	}
	return true;
}

static void codegen_inline_bytecode(struct token *tokens, size_t size) {
	size_t curr = 0;
	while (codegen_one_instruction(tokens, size, &curr)) {}
}

static void codegen_statement(void* expre) {
	if (!expre) return;
	struct statement* state = (struct statement*) expre;

	if (!get_settings_flag(SETTINGS_COMPILE)) {
		write_opcode(OP_SRC);
		write_integer(state->src_line);
	}
	switch (state->type) {
		case S_LET: {
			codegen_expr(state->op.let_statement.rvalue);
			// Request Memory
			if (get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
				if (state->op.let_statement.lvalue_offset.type == D_IDENTIFIER_LOCAL_OFFSET) {
					write_opcode(OP_LOFFSET);
					write_address(state->op.let_statement.lvalue_offset.value.number);
				}
				else if (state->op.let_statement.lvalue_offset.type == D_IDENTIFIER_GLOBAL_OFFSET) {
					write_opcode(OP_GOFFSET);
					write_address(state->op.let_statement.lvalue_offset.value.number);
				}
			}
			else {
				write_opcode(OP_DECL);
				write_string(state->op.let_statement.lvalue);
			}
			write_opcode(OP_WRITE);
			break;
		}
		case S_OPERATION: {
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
			break;
		}
		case S_EXPR: {
			codegen_expr(state->op.expr_statement);
			// Only output if it's not an assignment struct statement.
			if (state->op.expr_statement &&
				state->op.expr_statement->type != E_ASSIGN)
				write_opcode(OP_OUT);
			break;
		}
		case S_BYTECODE: {
			codegen_inline_bytecode(state->op.bytecode_statement.data,
				state->op.bytecode_statement.size);
			break;
		}
		case S_BREAK: {
			if (!current_loop_context) {
				error_lexer(state->src_line, 0, CODEGEN_BREAK_NOT_IN_LOOP);
				break;
			}
			if (scope_level < current_loop_context->scope) {
				error_general("Internal error, scope (%d) < current loop (%d)!",
					scope_level, current_loop_context->scope);
			}
			if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
				for (int j = scope_level; j != current_loop_context->scope; j--) {
					write_opcode(OP_END);
				}
			}
			write_opcode(OP_JMP);

			current_loop_context->break_locations[
				current_loop_context->break_count++] = size;

			size += sizeof(address);
			break;
		}
		case S_CONTINUE: {
			if (!current_loop_context) {
				error_lexer(state->src_line, 0, CODEGEN_CONTINUE_NOT_IN_LOOP);
				break;
			}
			if (scope_level < current_loop_context->scope) {
				error_general("Internal error, scope (%d) < current loop (%d)!",
					scope_level, current_loop_context->scope);
			}
			if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
				for (int j = scope_level; j != current_loop_context->scope; j--) {
					write_opcode(OP_END);
				}
			}
			write_opcode(OP_JMP);

			current_loop_context->continue_locations[
				current_loop_context->continue_count++] = size;

			size += sizeof(address);
			break;
		}
		case S_BLOCK: {
			if (state->op.block_statement) {
				make_scope();
				codegen_statement_list(state->op.block_statement);
				if (bytecode[size - 1] != OP_RET) {
					/* Don't need to end the block if we immediately RET */
					end_scope();
				}
				else {
					scope_level -= 1;
				}
			}
			break;
		}
		case S_IMPORT: {
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
					for (long i = verify_header(buffer, length); i < length; i++) {
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
			break;
		}
		case S_ENUM: {
			// We implement enums as a struct type. Each enum value
			// corresponds to a static instance of the type.

			// First, we make the struct prototype.
			size_t push_size = 5;
			char* enum_name = state->op.enum_statement.name;

			write_opcode(OP_PUSH);
			address meta_header_loc = size;
			write_data(make_data(D_STRUCT_HEADER, data_value_num(1)));
			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_NAME, data_value_str(enum_name)));
			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_SHARED, data_value_str("init")));
			codegen_expr(state->op.enum_statement.init_fn);

			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_PARAM, data_value_str("_num")));

			struct expr_list* curr = state->op.enum_statement.values;
			while (curr) {
				struct expr* elem = curr->elem;

				if (elem->type != E_LITERAL
					|| elem->op.lit_expr.type != D_IDENTIFIER) {
					error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
				}
				write_opcode(OP_PUSH);
				write_data(make_data(D_STRUCT_SHARED,
					data_value_str(elem->op.lit_expr.value.string)));

				// The value of it is an instance of the struct.
				write_opcode(OP_PUSH);
				write_data(none_data());

				push_size += 2;
				curr = curr->next;
			}

			write_opcode(OP_MKREF);
			write_byte(D_STRUCT);
			write_integer(push_size);

			write_opcode(OP_DECL);
			write_string(enum_name);
			write_opcode(OP_WRITE);
			write_double_at(push_size - 1, meta_header_loc + 1);

			// Now we assign each one.
			curr = state->op.enum_statement.values;
			size_t internal_num = 0;

			while (curr) {
				// Call Constructor
				codegen_end_marker();
				write_opcode(OP_PUSH);
				write_data(make_data(D_NUMBER, data_value_num(internal_num)));
				internal_num += 1;

				write_opcode(OP_PUSH);
				write_data(make_data(D_IDENTIFIER, data_value_str(enum_name)));
				write_opcode(OP_CALL);

				// Get LValue of Enum
				write_opcode(OP_PUSH);
				write_data(make_data(D_IDENTIFIER, data_value_str(enum_name)));
				write_opcode(OP_MEMPTR);
				write_string(curr->elem->op.lit_expr.value.string);
				write_opcode(OP_WRITE);
				curr = curr->next;
			}

			// Reset Constructor to be None
			write_opcode(OP_PUSH);
			write_data(none_data());
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(enum_name)));
			write_opcode(OP_MEMPTR);
			write_string("init");
			write_opcode(OP_WRITE);
			break;
		}
		case S_STRUCT: {
			// Header, name, init function.
			size_t push_size = 4;
			char* struct_name = state->op.struct_statement.name;

			// Push Header and Name
			write_opcode(OP_PUSH);
			address metaHeaderLoc = size;
			write_data(make_data(D_STRUCT_HEADER, data_value_num(1)));
			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_NAME, data_value_str(struct_name)));
			write_opcode(OP_PUSH);
			write_data(make_data(D_STRUCT_SHARED, data_value_str("init")));
			// Push Init Function
			codegen_expr(state->op.struct_statement.init_fn);

			struct expr_list* curr = state->op.struct_statement.instance_members;
			while (curr) {
				struct expr* elem = curr->elem;
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
				struct expr* elem = curr->elem;
				struct expr* rvalue = NULL;

				// Either a literal identifier or assignment statement.
				if (elem->type == E_ASSIGN && elem->op.assign_expr.operator == O_ASSIGN) {
					rvalue = elem->op.assign_expr.rvalue;
					elem = elem->op.assign_expr.lvalue;
				}

				if (elem->type != E_LITERAL
					|| elem->op.lit_expr.type != D_IDENTIFIER) {
					error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
				}
				write_opcode(OP_PUSH);
				write_data(make_data(D_STRUCT_SHARED,
					data_value_str(elem->op.lit_expr.value.string)));
				if (rvalue) {
					codegen_expr(rvalue);
				}
				else {
					write_opcode(OP_PUSH);
					write_data(none_data());
				}
				push_size += 2;
				curr = curr->next;
			}
			write_opcode(OP_MKREF);
			write_byte(D_STRUCT);
			write_integer(push_size);

			write_opcode(OP_DECL);
			write_string(struct_name);
			write_opcode(OP_WRITE);

			// header don't include the initial
			write_double_at(push_size - 1, metaHeaderLoc + 1);
			break;
		}
		case S_IF: {
			codegen_expr(state->op.if_statement.condition);
			write_opcode(OP_JIF);
			int falseJumpLoc = size;
			size += sizeof(address);

			make_scope();
			codegen_statement(state->op.if_statement.statement_true);
			end_scope();

			write_opcode(OP_JMP);
			int doneJumpLoc = size;
			size += sizeof(address);
			write_address_at(size, falseJumpLoc);

			make_scope();
			codegen_statement(state->op.if_statement.statement_false);
			end_scope();

			write_address_at(size, doneJumpLoc);
			break;
		}
		case S_LOOP: {
			if (!state->op.loop_statement.statement_true) {
				// Don't generate if empty loop body
				return;
			}
			make_scope();
			struct loop_context *new_ctx = safe_malloc(sizeof(struct loop_context));
			new_ctx->scope = scope_level;
			new_ctx->break_count = 0;
			new_ctx->continue_count = 0;
			new_ctx->parent = current_loop_context;
			current_loop_context = new_ctx;

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

				// container = <struct expr>
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
			address continue_addr = size;
			codegen_statement(state->op.loop_statement.post_loop);
			if (is_iterating_loop) {
				write_opcode(OP_WHERE);
				write_string(loop_index_name);
				write_opcode(OP_INC);
			}
			write_opcode(OP_JMP);
			write_address(loop_start_addr);
			write_address_at(size, loop_skip_loc);

			for (size_t i = 0; i < new_ctx->break_count; i++) {
				write_address_at(size, new_ctx->break_locations[i]);
			}

			for (size_t i = 0; i < new_ctx->continue_count; i++) {
				write_address_at(continue_addr, new_ctx->continue_locations[i]);
			}

			current_loop_context = new_ctx->parent;
			safe_free(new_ctx);
			end_scope();
			break;
		}
	}
}

static void codegen_statement_list(void* expre) {
	struct statement_list* list = (struct statement_list*) expre;
	while (list) {
		codegen_statement(list->elem);
		if (bytecode[size - 1] == OP_RET) {
			/* Stop generating if last struct statement was a return */
			break;
		}
		list = list->next;
	}
}

static void codegen_expr(void* expre) {
	if (!expre) return;
	struct expr* expression = (struct expr*)expre;
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
			assert(is_member_identifier(expression->op.bin_expr.right));
			write_opcode(OP_PUSH);
			write_data(copy_data(
                expression->op.bin_expr.right->op.lit_expr));
			codegen_expr(expression->op.bin_expr.left);
		}
		/* Short Circuiting for Boolean Operators. Boolean operators
		 * are commutative, so we can generate the left first. */
		else if (expression->op.bin_expr.operator == O_AND ||
				 expression->op.bin_expr.operator == O_OR) {
			codegen_expr(expression->op.bin_expr.left);
			bool is_or = expression->op.bin_expr.operator == O_OR;
			/* We want to negate for OR, so the JIF is accurate */
			if (is_or) {
				write_opcode(OP_UNA);
				write_byte(O_NOT);
			}
			write_opcode(OP_JIF);
			address short_circuit_loc = size;
			size += sizeof(address);
			/* LHS is True (or False for or), since we didn't short circuit */
			write_opcode(OP_PUSH);
			write_data(is_or ? false_data() : true_data());
			codegen_expr(expression->op.bin_expr.right);
			write_opcode(OP_BIN);
			write_byte(expression->op.bin_expr.operator);
			write_opcode(OP_JMP);
			address fine_loc = size;
			size += sizeof(address);

			/* Jump to Here if we short circuit */
			write_address_at(size, short_circuit_loc);
			write_opcode(OP_PUSH);
			write_data(is_or ? true_data() : false_data());

			/* Jump to Here if everything is fine */
			write_address_at(size, fine_loc);
			/* Skip the default OP_BIN */
			return;
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

		codegen_expr(expression->op.assign_expr.rvalue);

		if (op != O_ASSIGN) {
			codegen_expr(expression->op.assign_expr.lvalue);
		}

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

		bool made_new_frame = false;
		if (expression->op.call_expr.function->type == E_BINARY &&
			expression->op.call_expr.function->op.bin_expr.operator == O_MEMBER) {
			// Struct Member Call, we will generate an extra "argument" that is the
			//   reference to the LHS

			// Encase this in a seperate frame in case we use imports and it's
			//   already declared.
			if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
				write_opcode(OP_FRM);
			}
			made_new_frame = true;
			char temp_variable[30];
			sprintf(temp_variable, "$mem_call_tmp%zd", global_member_call_id++);
			codegen_expr(expression->op.call_expr.function->op.bin_expr.left);
			write_opcode(OP_DECL);
			write_string(temp_variable);
			write_opcode(OP_WRITE);
			write_opcode(OP_PUSH);
			write_data(make_data(D_IDENTIFIER, data_value_str(temp_variable)));

			traverse_expr(expression->op.call_expr.function->op.bin_expr.left, &ast_safe_free_impl);

			struct expr *temp_expr = safe_malloc(sizeof(struct expr));
			temp_expr->type = E_LITERAL;
			temp_expr->op.lit_expr = make_data(D_IDENTIFIER, data_value_str(temp_variable));
			expression->op.call_expr.function->op.bin_expr.left = temp_expr;
		}
		codegen_expr(expression->op.call_expr.function);
		write_opcode(OP_CALL);
		if (made_new_frame) {
			if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
				write_opcode(OP_END);
			}
		}
	}
	else if (expression->type == E_LIST) {
		int count = expression->op.list_expr.length;
		write_opcode(OP_PUSH);
		write_data(make_data(D_LIST_HEADER, data_value_num(count)));
		struct expr_list* param = expression->op.list_expr.contents;
		while (param) {
			codegen_expr(param->elem);
			param = param->next;
		}
		write_opcode(OP_MKREF);
		write_byte(D_LIST);
		write_integer(count + 1);
	}
	else if (expression->type == E_FUNCTION) {
		assert(verify_function_parameters(expression->op.func_expr.parameters));

		write_opcode(OP_JMP);
		int writeSizeLoc = size;
		size += sizeof(address);
		int startAddr = size;

		/* Count parameters */
		int count = 0;
		struct expr_list* param = expression->op.func_expr.parameters;
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

			struct expr_list* param = expression->op.func_expr.parameters;
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
			struct expr_list* param = expression->op.func_expr.parameters;
			bool has_encountered_default = false;
			int i = 0;
			size_t closure_count = 0;
			struct closure_mapping* curr = expression->op.func_expr.closure;

			while (curr) {
				closure_count++;
				curr = curr->next;
			}

			while (param) {
				if (param->elem->type == E_LITERAL) {
					if (has_encountered_default) {
						error_lexer(param->elem->line,
							param->elem->col,
							CODEGEN_FUNCTION_DEFAULT_VALUES_AT_END);
					}
					if (param->elem->op.lit_expr.type != D_PARAM_IDENTIFIER) {
						error_lexer(param->elem->line,
							param->elem->col,
							CODEGEN_UNEXPECTED_FUNCTION_PARAMETER);
					}
					struct data t = param->elem->op.lit_expr;
					if (!get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
						write_opcode(OP_DECL);
						write_string(t.value.string);
					} else {
						write_opcode(OP_LOFFSET);
						write_integer(i + 4);
					}
					write_opcode(OP_WRITE);
					param_names[i++] = t.value.string;
				}
				else if (param->elem->type == E_ASSIGN) {
					has_encountered_default = true;
					// Bind Default Value First
					codegen_expr(param->elem->op.assign_expr.rvalue);
					write_opcode(OP_DECL);
					// TODO: Check if assign struct expr is literal identifier.
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

		if (get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
			// The Closure List must be generated after the function PTR has been written
			// Generate Closure List for Function
			// The list looks like: Offset 1, Value 1, Offset 2, Value 2, ...
			struct closure_mapping* curr = expression->op.func_expr.closure;
			size_t count = 0;
			while (curr) {
				count += 2;
				curr = curr->next;
			}
			write_opcode(OP_PUSH);
			write_data(make_data(D_CLOSURE_HEADER, data_value_num(count)));
			curr = expression->op.func_expr.closure;
			while (curr) {
				write_opcode(OP_PUSH);
				write_data(make_data(D_NUMBER, data_value_num(curr->offset)));
				write_opcode(OP_PUSH);
				write_data(make_data(D_IDENTIFIER_LOCAL_OFFSET, data_value_num(curr->parent_offset)));
				curr = curr->next;
			}
			write_opcode(OP_MKREF);
			write_byte(D_CLOSURE);
			write_integer(count + 1);
		}
		else {
			write_opcode(OP_CLOSURE);
		}

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

uint8_t* generate_code(struct statement_list* _ast, size_t* size_ptr, id_list global_list) {
	UNUSED(global_list);
	if (current_loop_context) {
		error_general("Loop context exists already going into code generation!");
	}
	scope_level = 0;
	capacity = CODEGEN_START_SIZE;
	bytecode = safe_calloc(capacity, sizeof(uint8_t));
	size = 0;
	if (!get_settings_flag(SETTINGS_REPL)) {
		write_string(WENDY_VM_HEADER);
	}
	free_imported_libraries_ll();
	codegen_statement_list(_ast);
	free_imported_libraries_ll();
	write_opcode(OP_HALT);
	*size_ptr = size;

	if (global_list) {
		for (size_t i = 0; global_list[i]; i++) {
			safe_free(global_list[i]);
		}
	}
	return bytecode;
}

// CANNOT FREE OR DESTROY THIS ONE!
struct data get_data(uint8_t* bytecode, unsigned int* end) {
	// Bytecode is already Offset!
	struct data t;
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

void print_bytecode(uint8_t* bytecode, size_t length, FILE* buffer) {
	int max_len = 12;
	int baseaddr = 0;
	UNUSED(baseaddr);
	unsigned int i = 0;
	if (!get_settings_flag(SETTINGS_REPL)) {
		i = verify_header(bytecode, length);
	}

	fprintf(buffer, RED "WendyVM ByteCode Disassembly\n" GRN ".header\n");
	fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: ", &bytecode[0], 0);
	fprintf(buffer, YEL WENDY_VM_HEADER);
	fprintf(buffer, GRN "\n.code\n");
	forever {
		unsigned int start = i;
		fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: " RESET, &bytecode[i], i);
		enum opcode op = bytecode[i++];
		unsigned int p = 0;
		int printSourceLine = -1;
		p += fprintf(buffer, YEL "%6s " RESET, opcode_string[op]);

		switch (op) {
			case OP_PUSH: {
				struct data t = get_data(&bytecode[i], &i);
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
				enum operator o = bytecode[i++];
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
				enum data_type t = bytecode[i++];
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
			case OP_RET:
			case OP_WRITE:
			case OP_IN:
			case OP_OUT:
			case OP_OUTL:
			case OP_FRM:
			case OP_END:
			case OP_HALT:
			case OP_ARGCLN:
			case OP_CLOSURE:
			case OP_NTHPTR:
			case OP_INC:
			case OP_DEC:
			case OP_CALL:
				break;
			case OP_LOFFSET:
			case OP_GOFFSET: {
				p += fprintf(buffer, "%d", get_address(bytecode + i, &i));
			}
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

static void write_data_at_buffer(struct data t, uint8_t* buffer, size_t loc) {
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
	unsigned int i = 0;
	if (length > strlen(WENDY_VM_HEADER + 1) &&
		streq(WENDY_VM_HEADER, (char*)buffer)) {
		i += strlen(WENDY_VM_HEADER) + 1;
	}
	while (i < length) {
		enum opcode op = buffer[i++];
		switch (op) {
			case OP_PUSH: {
				size_t tokLoc = i;
				struct data t = get_data(buffer + i, &i);
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
