#include "ast.h"
#include "token.h"
#include "error.h"
#include "global.h"
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#define match(...) fnmatch(sizeof((enum token_type []) {__VA_ARGS__}) / sizeof(enum token_type), __VA_ARGS__)

static struct token* tokens = 0;
static size_t length = 0;
static size_t curr_index = 0;
static bool error_thrown = false;

// Forward Declarations
static struct expr* make_lit_expr(struct token t);
static struct expr* lit_expr_from_data(struct data d);
static struct expr* copy_lit_expr(struct expr* from);
static struct expr* make_bin_expr(struct expr* left, struct token op, struct expr* right);
static struct expr* make_una_expr(struct token op, struct expr* operand);
static struct expr* make_call_expr(struct expr* left, struct expr_list* arg_list);
static struct expr* make_list_expr(struct expr_list* list);
static struct expr* make_func_expr(struct expr_list* parameters, struct statement* body);
static struct expr* make_table_expr(struct expr_list* keys, struct expr_list* values);
static struct expr* make_native_func_expr(struct expr_list* parameters, struct token name);
static struct expr* make_assign_expr(struct expr* left, struct expr* right, struct token op);
static struct expr* make_if_expr(struct expr* condition, struct expr* if_true, struct expr* if_false);
static struct statement* parse_statement(void);
static struct statement_list* parse_statement_list(void);
static struct expr* expression(void);
static struct expr* or(void);
static bool check(enum token_type t);
static struct token advance(void);
static struct token previous(void);

// Public Methods
// Wrapping safe_free macro for use as a function pointer.

// In most cases we want traversal algorithm internals to be private, but these
// are exposed so optimizer can use them to properly free.
void ast_safe_free_e(struct expr* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
    if (ptr->type == E_FUNCTION && ptr->op.func_expr.is_native) {
        safe_free(ptr->op.func_expr.native_name);
    } else if (ptr->type == E_LITERAL) {
        destroy_data(&ptr->op.lit_expr);
    }
    safe_free(ptr);
}

void ast_safe_free_el(struct expr_list* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
    safe_free(ptr);
}

void ast_safe_free_s(struct statement* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
	switch(ptr->type) {
		case S_IMPORT:
			if (ptr->op.import_statement) {
				safe_free(ptr->op.import_statement);
			}
			break;
		case S_LET:
			if (ptr->op.let_statement.lvalue) {
				safe_free(ptr->op.let_statement.lvalue);
			}
			break;
		case S_BREAK:
		case S_CONTINUE:
			break;
		case S_BYTECODE:
			for (size_t i = 0; i < ptr->op.bytecode_statement.size; i++) {
				destroy_token(ptr->op.bytecode_statement.data[i]);
			}
			safe_free(ptr->op.bytecode_statement.data);
			break;
		case S_STRUCT:
			if (ptr->op.struct_statement.name) {
				safe_free(ptr->op.struct_statement.name);
			}
			break;
		case S_LOOP:
			if (ptr->op.loop_statement.index_var) {
				safe_free(ptr->op.loop_statement.index_var);
			}
			break;
		case S_ENUM:
			if (ptr->op.enum_statement.name) {
				safe_free(ptr->op.enum_statement.name);
			}
			break;
		case S_BLOCK:
		case S_EXPR:
		case S_IF:
		case S_OPERATION:
			break;
	}
    safe_free(ptr);
}

void ast_safe_free_sl(struct statement_list* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
    safe_free(ptr);
}

struct traversal_algorithm ast_safe_free_impl =
	TRAVERSAL_ALGO_POST (
		ast_safe_free_e,
		ast_safe_free_el,
		ast_safe_free_s,
		ast_safe_free_sl
	);

static void print_e(struct expr*, struct traversal_algorithm*);
static void print_el(struct expr_list*, struct traversal_algorithm*);
static void print_s(struct statement*, struct traversal_algorithm*);
static void print_sl(struct statement_list*, struct traversal_algorithm*);
static struct traversal_algorithm print_ast_impl =
	TRAVERSAL_ALGO_PRE(
		print_e,
		print_el,
		print_s,
		print_sl
	);

static bool is_at_end(void) {
	return curr_index == length;
}

struct statement_list* generate_ast(struct token* _tokens, size_t _length) {
	error_thrown = false;
	tokens = _tokens;
	length = _length;
	curr_index = 0;
	if (is_at_end()) return 0;
	return parse_statement_list();
}

void print_ast(struct statement_list* ast) {
	traverse_ast(ast, &print_ast_impl);
}

void free_ast(struct statement_list* ast) {
	traverse_ast(ast, &ast_safe_free_impl);
}

bool ast_error_flag(void) {
	return error_thrown;
}

static bool fnmatch(int count, ...) {
	va_list a_list;
	va_start(a_list, count);
	for (int i = 0; i < count; i++) {
		enum token_type next = va_arg(a_list, enum token_type);
		if (check(next)) {
			advance();
			return true;
		}
	}
	return false;
}

static struct token advance(void) {
	if (!is_at_end()) curr_index++;
	return previous();
}

static struct token previous(void) {
	return tokens[curr_index - 1];
}

static struct token peek(void) {
	return tokens[curr_index];
}

static bool check(enum token_type t) {
	if (is_at_end()) return false;
	return peek().t_type == t;
}

static void consume(enum token_type t) {
	if (check(t)) {
		advance();
	}
	else {
		struct token tok = previous();
		error_lexer(tok.t_line, tok.t_col, AST_EXPECTED_TOKEN,
			token_string[t]);
		advance();
		error_thrown = true;
		return;
	}
}

static struct expr_list* identifier_list(void) {
	struct expr_list* list = safe_malloc(sizeof(struct expr_list));
	list->next = 0;
	struct expr** curr = &list->elem;
	struct expr_list* curr_list = list;
	forever {
		if (match(T_IDENTIFIER)) {
			*curr = make_lit_expr(previous());
			if (match(T_COMMA)) {
				curr_list->next = safe_malloc(sizeof(struct expr_list));
				curr_list = curr_list->next;
				curr_list->next = 0;
				curr = &curr_list->elem;
			} else break;
		}
		else {
			struct token t = previous();
			error_lexer(t.t_line, t.t_col, AST_EXPECTED_IDENTIFIER);
			error_thrown = true;
		}
	}
	return list;
}

static struct expr_list* expression_list(enum token_type end_delimiter) {
	if (peek().t_type == end_delimiter) return 0;
	struct expr_list* list = safe_malloc(sizeof(struct expr_list));
	list->next = 0;
	struct expr** curr = &list->elem;
	struct expr_list* curr_list = list;
	forever {
		*curr = expression();
		if (match(T_COMMA)) {
			curr_list->next = safe_malloc(sizeof(struct expr_list));
			curr_list = curr_list->next;
			curr_list->next = 0;
			curr = &curr_list->elem;
		} else break;
	}
	if (error_thrown) {
		// rollback
		traverse_expr_list(list, &ast_safe_free_impl);
		return 0;
	}
	else {
		return list;
	}
}

static struct expr* lvalue(void) {
	struct expr* exp = or();
	return exp;
}

static struct expr* primary(void) {
	if (match(T_STRING, T_NUMBER, T_TRUE, T_FALSE, T_NONE, T_IDENTIFIER,
			T_OBJ_TYPE)) {
		return make_lit_expr(previous());
	}
	else if (match(T_LEFT_BRACK)) {
		struct expr_list* list = expression_list(T_RIGHT_BRACK);
		struct expr* list_expr = make_list_expr(list);
		consume(T_RIGHT_BRACK);
		return list_expr;
	}
	else if (match(T_LEFT_PAREN)) {
		struct expr* left = expression();
		consume(T_RIGHT_PAREN);
		return left;
	}
	else if (match(T_LAMBDA)) {
		consume(T_LEFT_PAREN);
		struct expr_list* list = expression_list(T_RIGHT_PAREN);
		consume(T_RIGHT_PAREN);
		struct statement* fn_body = parse_statement();
		return make_func_expr(list, fn_body);
	}
	else if (match(T_LEFT_BRACE)) {
		if (peek().t_type == T_RIGHT_BRACE) {
			consume(T_RIGHT_BRACE);
			return make_table_expr(NULL, NULL);
		}
		struct expr_list* keys = safe_malloc(sizeof(struct expr_list));
		struct expr_list* values = safe_malloc(sizeof(struct expr_list));
		keys->next = 0;
		values->next = 0;
		struct expr** curr_keys = &keys->elem;
		struct expr** curr_values = &values->elem;
		struct expr_list* curr_key_list = keys;
		struct expr_list* curr_val_list = values;
		forever {
			consume(T_IDENTIFIER);
			*curr_keys = make_lit_expr(previous());
			consume(T_COLON);
			*curr_values = expression();
			if (match(T_COMMA)) {
				curr_key_list->next = safe_malloc(sizeof(struct expr_list));
				curr_key_list = curr_key_list->next;
				curr_key_list->next = 0;
				curr_keys = &curr_key_list->elem;

				curr_val_list->next = safe_malloc(sizeof(struct expr_list));
				curr_val_list = curr_val_list->next;
				curr_val_list->next = 0;
				curr_values = &curr_val_list->elem;
			} else break;
		}
		consume(T_RIGHT_BRACE);
		return make_table_expr(keys, values);
	}
	else if (match(T_IF)) {
		struct expr* condition = expression();
		struct expr* run_if_true = expression();
		struct expr* run_if_false = 0;
		if (match(T_ELSE, T_COLON)) {
			run_if_false = expression();
		}
		return make_if_expr(condition, run_if_true, run_if_false);
	}
	else {
		struct token t = previous();
		error_lexer(t.t_line, t.t_col, AST_EXPECTED_PRIMARY);
		advance();
		error_thrown = true;
		return 0;
	}
}

static struct expr* access(void) {
	struct expr* left = primary();
	while (match(T_LEFT_BRACK, T_DOT, T_LEFT_PAREN)) {
		struct token op = previous();
		struct expr* right = 0;
		if (op.t_type == T_LEFT_BRACK) {
			right = expression();
			consume(T_RIGHT_BRACK);
			left = make_bin_expr(left, op, right);
		}
		else if (op.t_type == T_LEFT_PAREN) {
			struct expr_list* args = expression_list(T_RIGHT_PAREN);
			left = make_call_expr(left, args);
			consume(T_RIGHT_PAREN);
		}
		else {
			right = primary();
			left = make_bin_expr(left, op, right);
		}
	}
	return left;
}

static struct expr* unary(void) {
	if (match(T_MINUS, T_NOT, T_TILDE, T_DOT_DOT_DOT)) {
		struct token op = previous();
		struct expr* right = unary();
		return make_una_expr(op, right);
	}
	return access();
}
static struct expr* factor(void) {
	struct expr* left = unary();
	while (match(T_STAR, T_SLASH, T_INTSLASH, T_PERCENT, T_MOD_EQUAL, T_CARET)) {
		struct token op = previous();
		struct expr* right = unary();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static struct expr* term(void) {
	struct expr* left = factor();
	while (match(T_PLUS, T_MINUS)) {
		struct token op = previous();
		struct expr* right = factor();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static struct expr* range(void) {
	struct expr* left = term();
	while (match(T_RANGE_OP)) {
		struct token op = previous();
		struct expr* right = term();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static struct expr* comparison(void) {
	struct expr* left = range();
	while (match(T_NOT_EQUAL, T_EQUAL_EQUAL, T_LESS, T_GREATER, T_LESS_EQUAL,
					T_GREATER_EQUAL, T_TILDE)) {
		struct token op = previous();
		struct expr* right = range();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static struct expr* and(void) {
	struct expr* left = comparison();
	while (match(T_AND)) {
		struct token op = previous();
		struct expr* right = comparison();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static struct expr* or(void) {
	struct expr* left = and();
	while (match(T_OR)) {
		struct token op = previous();
		struct expr* right = and();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
static struct expr* assignment(void) {
	struct expr* left = lvalue();
	if (match(T_EQUAL, T_ASSIGN_PLUS, T_ASSIGN_MINUS,
		T_ASSIGN_STAR, T_ASSIGN_SLASH, T_ASSIGN_INTSLASH)) {
		struct token op = previous();
		struct expr* right = or();
		left = make_assign_expr(left, right, op);
	}
	else if (match(T_DEFFN)) {
        struct token op = previous();
		consume(T_LEFT_PAREN);
		struct expr_list* parameters = expression_list(T_RIGHT_PAREN);
		consume(T_RIGHT_PAREN);
		struct expr* rvalue;
		if (match(T_NATIVE)) {
			// Native Binding
			consume(T_IDENTIFIER);
			rvalue = make_native_func_expr(parameters, previous());
		}
		else {
			struct statement* fnbody = parse_statement();
			rvalue = make_func_expr(parameters, fnbody);
		}
		left = make_assign_expr(left, rvalue, op);
	}
	return left;
}
static struct expr* expression(void) {
	struct expr* res = assignment();
	if (error_thrown) {
		// Rollback
		traverse_expr(res, &ast_safe_free_impl);
		return 0;
	}
	else {
		return res;
	}
}

static struct statement* parse_statement(void) {
	struct token first = advance();
	struct statement* sm = safe_malloc(sizeof(struct statement));
	sm->src_line = first.t_line;
	switch (first.t_type) {
		case T_DOLLAR_SIGN: {
			consume(T_LEFT_BRACE);
			sm->type = S_BYTECODE;
			size_t start = curr_index;
			while (curr_index < length &&
				   tokens[curr_index].t_type != T_RIGHT_BRACE) {
				curr_index++;
			}
			size_t end = curr_index;
			consume(T_RIGHT_BRACE);
			struct token* bytecode_list = safe_malloc((end - start) * sizeof(struct token));
			for (size_t i = start; i < end; i++) {
				bytecode_list[i - start] = copy_token(tokens[i]);
			}
			sm->op.bytecode_statement.data = bytecode_list;
			sm->op.bytecode_statement.size = end - start;
			break;
		}
		case T_LEFT_BRACE: {
			if (peek().t_type == T_RIGHT_BRACE) {
				// Non-Empty struct statement Block
				consume(T_RIGHT_BRACE);
				safe_free(sm);
				return 0;
			}
			struct statement_list* sl = parse_statement_list();
			consume(T_RIGHT_BRACE);
			sm->type = S_BLOCK;
			sm->op.block_statement = sl;
			break;
		}
		case T_LET: {
			// Read an expression for a LVALUE
			char* lvalue = 0;
			if (match(T_IDENTIFIER)) {
                struct token prev = previous();
				lvalue = safe_strdup(prev.t_data.string);
			}
			else {
                size_t alloc_size = strlen(OPERATOR_OVERLOAD_PREFIX);
                bool is_binary = false;
                struct token lhs;
				if (match(T_OBJ_TYPE)) {
                    lhs = previous();
                    is_binary = true;
                    alloc_size += strlen(lhs.t_data.string);
                }
                struct token op = advance();
                struct token operand;
                if (precedence(op) || op.t_type == T_AT) {
                    alloc_size += strlen(op.t_data.string);
                    consume(T_OBJ_TYPE);
                    operand = previous();
                    alloc_size += strlen(operand.t_data.string);
					lvalue = safe_calloc(alloc_size + 1, sizeof(char));
					strcat(lvalue, OPERATOR_OVERLOAD_PREFIX);
					if (is_binary) {
						strcat(lvalue, lhs.t_data.string);
					}
					strcat(lvalue, op.t_data.string);
					strcat(lvalue, operand.t_data.string);
                }
                else {
					error_lexer(op.t_line, op.t_col,
						AST_OPERATOR_OVERLOAD_NO_OPERATOR);
				}
			}
			struct expr* rvalue = 0;
			if (match(T_EQUAL)) {
				rvalue = expression();
			}
			else if (match(T_DEFFN)) {
				consume(T_LEFT_PAREN);
				struct expr_list* parameters = expression_list(T_RIGHT_PAREN);
				consume(T_RIGHT_PAREN);
				if (match(T_NATIVE)) {
					// Native Binding
					consume(T_IDENTIFIER);
					rvalue = make_native_func_expr(parameters, previous());
				}
				else {
					struct statement* fnbody = parse_statement();
					rvalue = make_func_expr(parameters, fnbody);
				}
			}
			else {
				struct token none_tk = none_token();
				rvalue = make_lit_expr(none_tk);
				destroy_token(none_tk);
			}
			sm->type = S_LET;
			sm->op.let_statement.lvalue = lvalue;
			sm->op.let_statement.rvalue = rvalue;
			break;
		}
		case T_IF: {
			struct expr* condition = expression();
			struct statement* run_if_true = parse_statement();
			struct statement* run_if_false = 0;
			if (match(T_ELSE, T_COLON)) {
				run_if_false = parse_statement();
			}
			sm->type = S_IF;
			sm->op.if_statement.condition = condition;
			sm->op.if_statement.statement_true = run_if_true;
			sm->op.if_statement.statement_false = run_if_false;
			break;
		}
		case T_BREAK: {
			sm->type = S_BREAK;
			break;
		}
		case T_CONTINUE: {
			sm->type = S_CONTINUE;
			break;
		}
		case T_LOOP: {
			struct expr* index_var = expression();
			char* a_index;
			struct expr* condition;
			if (match(T_COLON, T_IN)) {
				condition = expression();
				if (index_var->type != E_LITERAL ||
						index_var->op.lit_expr.type != D_IDENTIFIER) {
					struct token t = previous();
					error_lexer(t.t_line, t.t_col, AST_EXPECTED_IDENTIFIER_LOOP);
				}
				a_index = safe_strdup(index_var->op.lit_expr.value.string);
		        traverse_expr(index_var, &ast_safe_free_impl);
			}
			else {
				condition = index_var;
				index_var = 0;
				a_index = 0;
			}
			struct statement* run_if_true = parse_statement();
			sm->type = S_LOOP;
			sm->op.loop_statement.condition = condition;
			sm->op.loop_statement.index_var = a_index;
			sm->op.loop_statement.statement_true = run_if_true;
			break;
		}
		case T_ENUM: {
			if (!match(T_IDENTIFIER)) {
				error_lexer(first.t_line, first.t_col,
					AST_ENUM_NAME_IDENTIFIER);
			}
			struct token name = previous();
			consume(T_DEFFN);
			struct expr_list* values = 0;
			match(T_LEFT_BRACE);
			if (!match(T_RIGHT_BRACE)) {
				values = identifier_list();
				consume(T_RIGHT_BRACE);
			}

			// Default Initiation Function, which is just "ret this"
			struct statement_list* init_fn = safe_malloc(sizeof(struct statement_list));
			struct statement_list* curr = init_fn;
			struct statement_list* prev = 0;

			// Simulate this._num = _num;
			curr->elem = safe_malloc(sizeof(struct statement));
			curr->elem->type = S_EXPR;
			curr->elem->op.expr_statement = safe_malloc(sizeof(struct expr));
			curr->elem->op.expr_statement->type = E_ASSIGN;
			struct expr* ass_expr = curr->elem->op.expr_statement;
			ass_expr->op.assign_expr.operator = O_ASSIGN;
			struct token num_token = make_token(T_IDENTIFIER, make_data_str("_num"));
			ass_expr->op.assign_expr.rvalue = make_lit_expr(num_token);
			// Binary Dot struct expr
			struct expr* left = lit_expr_from_data(make_data(D_IDENTIFIER,
							data_value_str("this")));
			struct expr* right = make_lit_expr(num_token);

			struct token op = make_token(T_DOT, make_data_str("."));
			// This isn't very clean, having to make a fake token to
			// construct the expression.
			ass_expr->op.assign_expr.lvalue = make_bin_expr(left, op, right);
			destroy_token(op);

			prev = curr;
			curr = safe_malloc(sizeof(struct statement_list));

			curr->elem = safe_malloc(sizeof(struct statement));
			prev->next = curr;
			curr->next = 0;
			curr->elem->type = S_OPERATION;
			curr->elem->op.operation_statement.operator = OP_RET;
			curr->elem->op.operation_statement.operand =
                lit_expr_from_data(make_data(D_IDENTIFIER,
								data_value_str("this")));

			struct statement* function_body = safe_malloc(sizeof(struct statement));
			function_body->type = S_BLOCK;
			function_body->op.block_statement = init_fn;
			// init_fn is now the list of statements, to make it a function
			struct expr_list* param_list = safe_malloc(sizeof(struct expr_list));
			param_list->next = 0;
			param_list->elem = make_lit_expr(num_token);
			struct expr* function_const = make_func_expr(param_list, function_body);

			sm->type = S_ENUM;
			sm->op.enum_statement.name = safe_strdup(name.t_data.string);
			sm->op.enum_statement.values = values;
			sm->op.enum_statement.init_fn = function_const;
			destroy_token(num_token);
			break;
		}
		case T_STRUCT: {
			if (!match(T_IDENTIFIER)) {
				error_lexer(first.t_line, first.t_col,
					AST_STRUCT_NAME_IDENTIFIER);
			}
			struct token name = previous();
			consume(T_DEFFN);
			struct expr_list* static_members = 0;
			struct expr_list* instance_members = 0;
			int saved_before_iden = 0;
			while (match(T_LEFT_PAREN, T_LEFT_BRACK, T_LEFT_BRACE)) {
				if (previous().t_type == T_LEFT_PAREN) {
					saved_before_iden = curr_index;
					if (match(T_RIGHT_PAREN)) continue;
					instance_members = identifier_list();
					consume(T_RIGHT_PAREN);
				}
				else if (previous().t_type == T_LEFT_BRACK) {
					if (match(T_RIGHT_BRACK)) continue;
					static_members = expression_list(T_RIGHT_BRACK);
					consume(T_RIGHT_BRACK);
				}
				else if (previous().t_type == T_LEFT_BRACE) {
					if (match(T_RIGHT_BRACE)) continue;
					// Struct functions:
					//   fn => (params) { function };
					// Write this into static member list as
					//   as an assign_expression
					while (match(T_IDENTIFIER)) {
						struct expr* lvalue = make_lit_expr(previous());
						consume(T_DEFFN);
						consume(T_LEFT_PAREN);
						struct expr_list* parameters = expression_list(T_RIGHT_PAREN);
						consume(T_RIGHT_PAREN);
						struct statement* fnbody = parse_statement();
						struct expr* fn = make_func_expr(parameters, fnbody);
						struct expr_list* new_static = safe_malloc(sizeof(struct expr_list));
						new_static->next = static_members;
						static_members = new_static;
						new_static->elem = make_assign_expr(lvalue, fn, make_token(T_EQUAL, make_data_num(0)));
					}
					consume(T_RIGHT_BRACE);
				}
			}
			// Default Initiation Function
			struct statement_list* init_fn = safe_malloc(sizeof(struct statement_list));
			struct statement_list* curr = init_fn;
			struct statement_list* prev = 0;

			struct expr_list* tmp_ins = instance_members;
			while (tmp_ins) {
				curr->elem = safe_malloc(sizeof(struct statement));
				curr->elem->type = S_EXPR;
				curr->elem->op.expr_statement = safe_malloc(sizeof(struct expr));
				curr->elem->op.expr_statement->type = E_ASSIGN;
				struct expr* ass_expr = curr->elem->op.expr_statement;
				ass_expr->op.assign_expr.operator = O_ASSIGN;
				ass_expr->op.assign_expr.rvalue = copy_lit_expr(tmp_ins->elem);
				// Binary Dot struct expr
				struct expr* left = lit_expr_from_data(make_data(D_IDENTIFIER,
								data_value_str("this")));
				struct expr* right = copy_lit_expr(tmp_ins->elem);

				struct token op = make_token(T_DOT, make_data_str("."));
                // This isn't very clean, having to make a fake token to
                // construct the expression.
				ass_expr->op.assign_expr.lvalue = make_bin_expr(left, op, right);
                destroy_token(op);

				if (prev) prev->next = curr;
				prev = curr;
				curr = safe_malloc(sizeof(struct statement_list));

				tmp_ins = tmp_ins->next;
			}
			// Return This Operation
			curr->elem = safe_malloc(sizeof(struct statement));
			if(prev) prev->next = curr;
			curr->next = 0;
			curr->elem->type = S_OPERATION;
			curr->elem->op.operation_statement.operator = OP_RET;
			curr->elem->op.operation_statement.operand =
                lit_expr_from_data(make_data(D_IDENTIFIER,
								data_value_str("this")));

			struct expr_list* parameters = 0;
			if (instance_members) {
				size_t saved_before_pop = curr_index;
				curr_index = saved_before_iden;
				parameters = identifier_list();
				curr_index = saved_before_pop;
			}
			struct statement* function_body = safe_malloc(sizeof(struct statement));
			function_body->type = S_BLOCK;
			function_body->op.block_statement = init_fn;
			// init_fn is now the list of statements, to make it a function
			struct expr* function_const = make_func_expr(parameters, function_body);

			sm->type = S_STRUCT;
			sm->op.struct_statement.name = safe_strdup(name.t_data.string);
			sm->op.struct_statement.init_fn = function_const;
			sm->op.struct_statement.instance_members = instance_members;
			sm->op.struct_statement.static_members = static_members;
			break;
		}
		case T_INC:
		case T_DEC:
		case T_INPUT:
		case T_AT:  {
			sm->type = S_OPERATION;
			// Initializing this for the default case, even though the default case should
			//   never be reached, because we check for the t_type above.
            enum opcode code = OP_PUSH;
            switch(first.t_type) {
                case T_INC: code = OP_INC; break;
                case T_DEC: code = OP_DEC; break;
                case T_INPUT: code = OP_IN; break;
                case T_AT: code = OP_OUTL; break;
                default: break;
            }
			sm->op.operation_statement.operator = code;
			sm->op.operation_statement.operand = expression();
			break;
		}
		case T_REQ: {
			sm->type = S_IMPORT;
			if (match(T_IDENTIFIER, T_STRING)) {
				// Don't actually need to do anything if it's a string, but we
				//   delegate that task to codegen to decide.
                struct token p = previous();
                if (p.t_type == T_IDENTIFIER) {
				    sm->op.import_statement = safe_strdup(p.t_data.string);
                } else {
                    sm->op.import_statement = 0;
                }
			}
			else {
				error_lexer(first.t_line, first.t_col, AST_UNRECOGNIZED_IMPORT);
			}
			break;
		}
		case T_RET: {
			sm->type = S_OPERATION;
			sm->op.operation_statement.operator = OP_RET;
			if (peek().t_type != T_SEMICOLON && peek().t_type != T_RIGHT_BRACE) {
				sm->op.operation_statement.operand = expression();
			}
			else {
				sm->op.operation_statement.operand = lit_expr_from_data(noneret_data());
			}
			break;
		}
		default: {
			// We advanced it so we gotta roll back.
			curr_index--;
			// Handle as expression.
			sm->type = S_EXPR;
			sm->op.expr_statement = expression();
		}
	}
	match(T_SEMICOLON);
	if (error_thrown) {
		// Rollback
		traverse_statement(sm, &ast_safe_free_impl);
		return 0;
	}
	return sm;
}

static struct statement_list* parse_statement_list(void) {
	struct statement_list* ast = safe_malloc(sizeof(struct statement_list));
	ast->next = 0;
	struct statement** curr = &ast->elem;
	struct statement_list* curr_ast = ast;
	while (true) {
		*curr = parse_statement();
		if (!is_at_end() && peek().t_type != T_RIGHT_BRACE) {
			curr_ast->next = safe_malloc(sizeof(struct statement_list));
			curr_ast = curr_ast->next;
			curr_ast->next = 0;
			curr = &curr_ast->elem;
		} else break;
	}
	if (error_thrown) {
		// Rollback
		traverse_statement_list(ast, &ast_safe_free_impl);
		return 0;
	}
	else {
		return ast;
	}
}

void traverse_statement_list(struct statement_list* list, struct traversal_algorithm* algo) {
	if (!list) return;
	if (algo->handle_statement_list_pre) {
		algo->handle_statement_list_pre(list, algo);
	}
	algo->level++;
	traverse_statement(list->elem, algo);
	algo->level--;
	traverse_statement_list(list->next, algo);
	if (algo->handle_statement_list_post) {
		algo->handle_statement_list_post(list, algo);
	}
}

void traverse_statement(struct statement* state, struct traversal_algorithm* algo) {
	if (!state) return;
	if (algo->handle_statement_pre) {
		algo->handle_statement_pre(state, algo);
	}
	algo->level++;
	switch(state->type) {
		case S_LET: {
			traverse_expr(state->op.let_statement.rvalue, algo);
			break;
		}
		case S_OPERATION: {
			traverse_expr(state->op.operation_statement.operand, algo);
			break;
		}
		case S_EXPR: {
			traverse_expr(state->op.expr_statement, algo);
			break;
		}
		case S_BREAK:
		case S_CONTINUE:
		case S_BYTECODE: {
			break;
		}
		case S_BLOCK: {
			traverse_statement_list(state->op.block_statement, algo);
			break;
		}
		case S_STRUCT: {
			traverse_expr(state->op.struct_statement.init_fn, algo);
			traverse_expr_list(state->op.struct_statement.instance_members, algo);
			traverse_expr_list(state->op.struct_statement.static_members, algo);
			break;
		}
		case S_ENUM: {
			traverse_expr(state->op.enum_statement.init_fn, algo);
			traverse_expr_list(state->op.enum_statement.values, algo);
			break;
		}
		case S_IF: {
			traverse_expr(state->op.if_statement.condition, algo);
			traverse_statement(state->op.if_statement.statement_true, algo);
			traverse_statement(state->op.if_statement.statement_false, algo);
			break;
		}
		case S_LOOP: {
			traverse_expr(state->op.loop_statement.condition, algo);
			traverse_statement(state->op.loop_statement.statement_true, algo);
			break;
		}
		case S_IMPORT: break;
	}
	algo->level--;
	if (algo->handle_statement_post) {
		algo->handle_statement_post(state, algo);
	}
}

void traverse_expr(struct expr* expression, struct traversal_algorithm* algo) {
	if (!expression) return;
	if (algo->handle_expr_pre) {
		algo->handle_expr_pre(expression, algo);
	}
	algo->level++;
	switch (expression->type) {
		case E_LITERAL: {
			break;
		}
		case E_BINARY: {
			traverse_expr(expression->op.bin_expr.left, algo);
			traverse_expr(expression->op.bin_expr.right, algo);
			break;
		}
		case E_IF: {
			traverse_expr(expression->op.if_expr.condition, algo);
			traverse_expr(expression->op.if_expr.expr_true, algo);
			traverse_expr(expression->op.if_expr.expr_false, algo);
			break;
		}
		case E_UNARY: {
			traverse_expr(expression->op.una_expr.operand, algo);
			break;
		}
		case E_CALL: {
			traverse_expr(expression->op.call_expr.function, algo);
			traverse_expr_list(expression->op.call_expr.arguments, algo);
			break;
		}
		case E_LIST: {
			traverse_expr_list(expression->op.list_expr.contents, algo);
			break;
		}
		case E_FUNCTION: {
			traverse_expr_list(expression->op.func_expr.parameters, algo);
			traverse_statement(expression->op.func_expr.body, algo);
			break;
		}
		case E_TABLE: {
			traverse_expr_list(expression->op.table_expr.keys, algo);
			traverse_expr_list(expression->op.table_expr.values, algo);
			break;
		}
		case E_ASSIGN: {
			traverse_expr(expression->op.assign_expr.lvalue, algo);
			traverse_expr(expression->op.assign_expr.rvalue, algo);
			break;
		}
	}
	algo->level--;
	if (algo->handle_expr_post) {
		algo->handle_expr_post(expression, algo);
	}
}

void traverse_expr_list(struct expr_list* list, struct traversal_algorithm* algo) {
	if (!list) return;
	if (algo->handle_expr_list_pre) {
		algo->handle_expr_list_pre(list, algo);
	}
	algo->level++;
	traverse_expr(list->elem, algo);
	algo->level--;
	traverse_expr_list(list->next, algo);
	if (algo->handle_expr_list_post) {
		algo->handle_expr_list_post(list, algo);
	}
}

static void print_indent(struct traversal_algorithm* algo) {
	for (int i = 0; i < algo->level; i++) {
		printf("| ");
	}
	printf("`-");
}

static void print_e(struct expr* expression, struct traversal_algorithm* algo) {
	print_indent(algo);
	printf("%s", YEL);
	switch (expression->type) {
		case E_LITERAL: {
			printf("Literal Expression " GRN);
			print_data(&expression->op.lit_expr);
			printf("%s", RESET);
			break;
		}
		case E_BINARY: {
			printf("Binary Expression " GRN "%s\n" RESET,
				operator_string[expression->op.bin_expr.operator]);
			break;
		}
		case E_IF: {
			printf("Conditional Expression\n");
			break;
		}
		case E_UNARY: {
			printf("Unary Expression\n");
			break;
		}
		case E_CALL: {
			printf("Call Expression\n");
			break;
		}
		case E_LIST: {
			printf("List Expression\n");
			break;
		}
		case E_FUNCTION: {
			printf("Function Expression\n");
			break;
		}
		case E_TABLE: {
			printf("Table Expression\n");
			break;
		}
		case E_ASSIGN: {
			printf("Assignment Expression \n");
			break;
		}
	}
	printf("%s", RESET);
}
static void print_el(struct expr_list* el, struct traversal_algorithm* algo) {
	UNUSED(el);
	print_indent(algo);
	printf(CYN "<Expression List Item>\n" RESET);
}
static void print_s(struct statement* state, struct traversal_algorithm* algo) {
	print_indent(algo);
	printf("%s", BLU);
	switch (state->type) {
		case S_LET: {
			printf("Let statement " GRN "(%s)\n" RESET,
				state->op.let_statement.lvalue);
			break;
		}
		case S_OPERATION: {
			printf("Operation statement " GRN "%s\n" RESET,
				operator_string[state->op.operation_statement.operator]);
			break;
		}
		case S_EXPR: {
			printf("Expression statement\n");
			break;
		}
		case S_BYTECODE: {
			printf("Inline bytecode statement\n");
			break;
		}
		case S_BREAK: {
			printf("Break statement\n");
			break;
		}
		case S_CONTINUE: {
			printf("Continue statement\n");
			break;
		}
		case S_BLOCK: {
			printf("Block statement \n");
			break;
		}
		case S_STRUCT: {
			printf("Struct statement " GRN "%s\n" RESET,
				state->op.struct_statement.name);
			break;
		}
		case S_ENUM: {
			printf("Enum statement " GRN "%s\n" RESET,
				state->op.enum_statement.name);
			break;
		}
		case S_IF: {
			printf("If statement\n");
			break;
		}
		case S_LOOP: {
			printf("Loop statement\n");
			break;
		}
		case S_IMPORT: {
			printf("Import statement\n");
			break;
		}
	}
	printf("%s", RESET);
}
static void print_sl(struct statement_list* sl, struct traversal_algorithm* algo) {
	UNUSED(sl);
	print_indent(algo);
	printf(MAG "<Statement List Item %p>\n" RESET, sl);
}

void traverse_ast(struct statement_list* list, struct traversal_algorithm* algo) {
	traverse_statement_list(list, algo);
}

/* Consumes d */
static struct expr* lit_expr_from_data(struct data d) {
    struct expr* node = safe_malloc(sizeof(struct expr));
    node->type = E_LITERAL;
    node->op.lit_expr = d;
    return node;
}

static struct expr* copy_lit_expr(struct expr* from) {
    struct expr* node = lit_expr_from_data(copy_data(from->op.lit_expr));
	node->line = from->line;
	node->col = from->col;
	return node;
}
static struct expr* make_lit_expr(struct token t) {
	struct expr* node = lit_expr_from_data(literal_to_data(t));
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static struct expr* make_bin_expr(struct expr* left, struct token op, struct expr* right) {
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->line = op.t_line;
	node->col = op.t_col;
	node->type = E_BINARY;
	node->op.bin_expr.operator = token_operator_binary(op);
	node->op.bin_expr.left = left;
	node->op.bin_expr.right = right;
	return node;
}
static struct expr* make_if_expr(struct expr* condition, struct expr* if_true, struct expr* if_false) {
	struct token t = tokens[curr_index];
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->line = t.t_line;
	node->col = t.t_col;
	node->type = E_IF;
	node->op.if_expr.expr_true = if_true;
	node->op.if_expr.condition = condition;
	node->op.if_expr.expr_false = if_false;
	return node;
}
static struct expr* make_una_expr(struct token op, struct expr* operand) {
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->type = E_UNARY;
	node->op.una_expr.operator = token_operator_unary(op);
	node->op.una_expr.operand = operand;
	node->line = op.t_line;
	node->col = op.t_col;
	return node;
}
static struct expr* make_call_expr(struct expr* left, struct expr_list* arg_list) {
	struct token t = tokens[curr_index];
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->type = E_CALL;
	node->op.call_expr.function = left;
	node->op.call_expr.arguments = arg_list;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static struct expr* make_list_expr(struct expr_list* list) {
	struct token t = tokens[curr_index];
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->type = E_LIST;
	int size = 0;
	struct expr_list* start = list;
	while (start) {
		start = start->next;
		size++;
	}
	node->op.list_expr.length = size;
	node->op.list_expr.contents = list;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static struct expr* make_assign_expr(struct expr* left, struct expr* right, struct token op) {
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->type = E_ASSIGN;
	node->op.assign_expr.lvalue = left;
	node->op.assign_expr.rvalue = right;
    enum operator new_op = O_ASSIGN;
    switch(op.t_type) {
        case T_ASSIGN_PLUS:
            new_op = O_ADD;
            break;
        case T_ASSIGN_MINUS:
            new_op = O_SUB;
            break;
        case T_ASSIGN_STAR:
            new_op = O_MUL;
            break;
        case T_ASSIGN_SLASH:
            new_op = O_DIV;
            break;
        case T_ASSIGN_INTSLASH:
            new_op = O_IDIV;
            break;
        case T_EQUAL:
        case T_DEFFN:
        default: break;
    }
	node->op.assign_expr.operator = new_op;
	node->line = op.t_line;
	node->col = op.t_col;
	return node;
}
static struct expr* make_func_expr(struct expr_list* parameters, struct statement* body) {
	struct token t = tokens[curr_index];
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->type = E_FUNCTION;
	node->op.func_expr.parameters = parameters;
	node->op.func_expr.body = body;
	node->op.func_expr.is_native = false;
	node->line = t.t_line;
	node->col = t.t_col;
	return node;
}
static struct expr* make_table_expr(struct expr_list* keys, struct expr_list* values) {
	struct expr* node = safe_malloc(sizeof(struct expr));
	node->type = E_TABLE;
	node->op.table_expr.keys = keys;
	node->op.table_expr.values = values;
	return node;
}
static struct expr* make_native_func_expr(struct expr_list* parameters, struct token name) {
	struct expr* node = make_func_expr(parameters, 0);
	node->op.func_expr.is_native = true;
	node->op.func_expr.native_name = safe_strdup(name.t_data.string);
	return node;
}
