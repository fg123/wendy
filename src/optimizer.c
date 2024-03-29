#include "optimizer.h"
#include "ast.h"
#include "global.h"
#include "error.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// Implementation of Optimization Algorithms
// Note: this file is particularly messy because the normal AST traversal
//   algorithm doesn't apply here. The optimizer passes does not only read from
//   the AST, but readily mutates it as well.

// Linked list node of identifiers within a struct statement block
struct id_node {
	char* id_name;
	struct expr* value;
	int modified_count;
	int usage_count;
	struct id_node* next;
};

// struct statement scope block to simulate function calls and checking if identifiers exist.
struct statement_block {
	struct id_node* id_list;
	struct statement_block* next;
};

static struct statement_block* curr_statement_block = 0;

// Forward Declarations
static void free_id_nodes(struct id_node* node);
static void free_statement_blocks(struct statement_block* block);
static struct statement_list* optimize_statement_list(struct statement_list* list);
static struct statement* optimize_statement(struct statement* state);
static struct expr* optimize_expr(struct expr* expression);
static struct expr_list* optimize_expr_list(struct expr_list* list);
static struct statement_list* scan_statement_list_with_new_block(struct statement_list* list);
static void scan_statement_list(struct statement_list* list);
static void scan_statement(struct statement* state);
static void scan_expr(struct expr* expression);
static void scan_expr_list(struct expr_list* list);

static inline bool is_boolean(struct data t) {
	return t.type == D_TRUE || t.type == D_FALSE;
}

static struct id_node* find_id_node(char* id, int line, int col) {
	UNUSED(line);
	UNUSED(col);
	// Search in each block and each list. Returns null if not found for now.
	// TODO: perform identifier checking here, but will need to add in
	//   special cases like `this` for OOP
	struct statement_block* b = curr_statement_block;
	while (b) {
		struct id_node* n = b->id_list;
		while (n) {
			if (streq(n->id_name, id)) {
				return n;
			}
			n = n->next;
		}
		b = b->next;
	}
	return 0;
}

static void remove_entry(char* id, int line, int col) {
	UNUSED(line);
	UNUSED(col);
	// Basic LL Search and Remove
	struct statement_block* b = curr_statement_block;
	while (b) {
		struct id_node** n = &b->id_list;
		while (*n) {
			if (streq((*n)->id_name, id)) {
				struct id_node* next = (*n)->next;
				safe_free(*n);
				*n = next;
				return;
			}
			n = &((*n)->next);
		}
		b = b->next;
	}
}

static void add_usage(char* id, int line, int col) {
	struct id_node* r = find_id_node(id, line, col);
	if (r) r->usage_count++;
}

static void remove_usage(char* id, int line, int col) {
	struct id_node* r = find_id_node(id, line, col);
	if (r) r->usage_count--;
}

static int get_usage(char* id, int line, int col) {
	struct id_node* r = find_id_node(id, line, col);
	if (r) return r->usage_count;
	else return 100;
}

static struct expr* get_value(char* id, int line, int col) {
	struct id_node* r = find_id_node(id, line, col);
	if (r) return r->value;
	else return 0;
}

static int get_modified(char* id, int line, int col) {
	struct id_node* r = find_id_node(id, line, col);
	if (r) return r->modified_count;
	else return 100;
}

static void add_modified(char* id, int line, int col) {
	struct id_node* r = find_id_node(id, line, col);
	if (r) r->modified_count++;
}

void optimize_safe_free_e(struct expr* expression, struct traversal_algorithm* algo) {
	UNUSED(algo);
	if (expression->type == E_LITERAL
		&& expression->op.lit_expr.type == D_IDENTIFIER) {
		remove_usage(expression->op.lit_expr.value.string,
				expression->line, expression->col);
	}
    ast_safe_free_e(expression, algo);
}
void optimize_safe_free_el(struct expr_list* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
    ast_safe_free_el(ptr, algo);
}

void optimize_safe_free_s(struct statement* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
    ast_safe_free_s(ptr, algo);
}

void optimize_safe_free_sl(struct statement_list* ptr, struct traversal_algorithm* algo) {
    UNUSED(algo);
    ast_safe_free_sl(ptr, algo);
}

struct traversal_algorithm optimize_safe_free_impl = TRAVERSAL_ALGO_POST(
	optimize_safe_free_e,
	optimize_safe_free_el,
	optimize_safe_free_s,
	optimize_safe_free_sl);

struct statement_list* optimize_ast(struct statement_list* ast) {
	struct statement_list* result = scan_statement_list_with_new_block(ast);
	free_statement_blocks(curr_statement_block);
	return result;
}

static void free_statement_block(struct statement_block* block) {
	if (!block) return;
	free_id_nodes(curr_statement_block->id_list);
	safe_free(block);
}

void free_statement_blocks(struct statement_block* block) {
	if (!block) return;
	free_statement_blocks(block->next);
	free_statement_block(block);
}

static void free_id_nodes(struct id_node* node) {
	if (!node) return;
	free_id_nodes(node->next);
	safe_free(node);
}

static void add_node(char* id, struct expr* value) {
	if (!curr_statement_block) {
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK);
		return;
	}
	struct id_node* new_node = safe_malloc(sizeof(struct id_node));
	new_node->id_name = id;
	new_node->value = value;
	new_node->modified_count = 0;
	new_node->usage_count = 0;
	new_node->next = curr_statement_block->id_list;
	curr_statement_block->id_list = new_node;
}

static void make_new_block(void) {
	struct statement_block* new_block = safe_malloc(sizeof(struct statement_block));
	new_block->next = curr_statement_block;
	new_block->id_list = 0;
	curr_statement_block = new_block;
}

static void delete_block(void) {
	struct statement_block* next = curr_statement_block->next;
	free_statement_block(curr_statement_block);
	curr_statement_block = next;
}

static struct statement_list* scan_optimize_statement_list(struct statement_list* list) {
	// We run two passes of optimization for some unknown reason. I added this
	//   back in the day and I can't remember why :)
	scan_statement_list(list);
	list = optimize_statement_list(list);
	list = optimize_statement_list(list);
	return list;
}
static struct statement_list* scan_statement_list_with_new_block(struct statement_list* list) {
	make_new_block();
	list = scan_optimize_statement_list(list);
	delete_block();
	return list;
}

/* OPTIMIZE CODE */
static struct statement* optimize_statement(struct statement* state) {
	if (!state) return 0;
	switch (state->type) {
		case S_LET: {
			state->op.let_statement.rvalue =
					optimize_expr(state->op.let_statement.rvalue);
			if (get_usage(state->op.let_statement.lvalue,
					state->src_line,
					0) == 0 &&
				state->op.let_statement.rvalue->type == E_LITERAL) {
				remove_entry(state->op.let_statement.lvalue,
					state->src_line,
					0);
				traverse_statement(state, &optimize_safe_free_impl);

				return 0;
			}
			break;
		}
		case S_BREAK:
		case S_CONTINUE:
		case S_BYTECODE:
		case S_OPERATION: {
			break;
		}
		case S_EXPR: {
			state->op.expr_statement = optimize_expr(state->op.expr_statement);
			break;
		}
		case S_BLOCK: {
			if (!state->op.block_statement) {
				return 0;
			}
			break;
		}
		case S_STRUCT: {
			state->op.struct_statement.init_fn =
				optimize_expr(state->op.struct_statement.init_fn);
			state->op.struct_statement.instance_members =
				optimize_expr_list(state->op.struct_statement.instance_members);
			state->op.struct_statement.static_members =
				optimize_expr_list(state->op.struct_statement.static_members);
			break;
		}
		case S_ENUM: {
			state->op.enum_statement.values =
				optimize_expr_list(state->op.enum_statement.values);
			break;
		}
		case S_IF: {
			state->op.if_statement.condition =
				optimize_expr(state->op.if_statement.condition);
			struct expr* condition = state->op.if_statement.condition;

			state->op.if_statement.statement_true =
				optimize_statement(state->op.if_statement.statement_true);
			state->op.if_statement.statement_false =
				optimize_statement(state->op.if_statement.statement_false);

			struct statement* run_if_false = state->op.if_statement.statement_false;
			struct statement* run_if_true = state->op.if_statement.statement_true;
			if (condition->type == E_LITERAL && condition->op.lit_expr.type == D_TRUE) {
				// Always going to be true!
				traverse_statement(run_if_false, &optimize_safe_free_impl);
				traverse_expr(condition, &optimize_safe_free_impl);
				safe_free(state);
				return run_if_true;
			}
			else if (condition->type == E_LITERAL && condition->op.lit_expr.type == D_FALSE) {
				traverse_statement(run_if_true, &optimize_safe_free_impl);
				traverse_expr(condition, &optimize_safe_free_impl);
				safe_free(state);
				return run_if_false;
			}
			break;
		}
		case S_LOOP: {
			state->op.loop_statement.condition =
				optimize_expr(state->op.loop_statement.condition);
			state->op.loop_statement.statement_true =
				optimize_statement(state->op.loop_statement.statement_true);
			if (!state->op.loop_statement.statement_true) {
				// Empty Body
				traverse_expr(state->op.loop_statement.condition, &optimize_safe_free_impl);
				return 0;
			}
			break;
		}
		case S_IMPORT: break;
	}
	return state;
}

static struct statement_list* optimize_statement_list(struct statement_list* list) {
	if (!list) return 0;
	list->next = optimize_statement_list(list->next);
	list->elem = optimize_statement(list->elem);
	if (list->elem) {
		return list;
	}
	else {
		// No struct statement
		struct statement_list* next = list->next;
		safe_free(list);
		return next;
	}
}

static struct expr* optimize_expr(struct expr* expression) {
	if (!expression) return 0;
	switch (expression->type) {
		case E_LITERAL: {
			if (expression->op.lit_expr.type == D_IDENTIFIER) {
				// Is a static, then replace with value if literal
				if (get_modified(expression->op.lit_expr.value.string,
					expression->line, expression->col) == 0) {
					struct expr* value = get_value(expression->op.lit_expr.value.string,
						expression->line, expression->col);
					if (value && value->type == E_LITERAL) {
						remove_usage(expression->op.lit_expr.value.string,
							expression->line, expression->col);
						destroy_data(&expression->op.lit_expr);
						expression->op.lit_expr = copy_data(value->op.lit_expr);
					}
				}
			}
			break;
		}
		case E_BINARY: {
			expression->op.bin_expr.left =
				optimize_expr(expression->op.bin_expr.left);
			expression->op.bin_expr.right =
				optimize_expr(expression->op.bin_expr.right);

			struct expr* left = expression->op.bin_expr.left;
			struct expr* right = expression->op.bin_expr.right;
			enum vm_operator op = expression->op.bin_expr.vm_operator;
			struct data possible_optimized;
			if (left->type == E_LITERAL && left->op.lit_expr.type == D_NUMBER &&
				right->type == E_LITERAL && right->op.lit_expr.type == D_NUMBER) {
				// Peek Optimization is Available on Numbers
				// Optimized Reuslt will be on OP
				bool can_optimize = true;
				double a = left->op.lit_expr.value.number;
				double b = right->op.lit_expr.value.number;
				switch (op) {
					case O_MUL:
						possible_optimized.value.number = a * b;
						break;
					case O_IDIV:
						if (b != 0) {
							possible_optimized.value.number = (int)(a / b);
						}
						break;
					case O_DIV:
						if (b != 0) {
							possible_optimized.value.number = a / b;
						}
						break;
					case O_REM:
						if (b != 0 && a == floor(a) && b == floor(b)) {
							possible_optimized.value.number = (long long)a % (long long)b;
						}
						else if (b != 0) {
							possible_optimized.value.number = fmod(a, b);
						}
						else {
							can_optimize = false;
						}
						break;
					case O_SUB:
						possible_optimized.value.number = a - b;
						break;
					case O_ADD:
						possible_optimized.value.number = a + b;
						break;
					case O_LT:
						possible_optimized = a < b ? true_data() : false_data();
						break;
					case O_GT:
						possible_optimized = a > b ? true_data() : false_data();
						break;
					case O_LTE:
						possible_optimized = a <= b ? true_data() : false_data();
						break;
					case O_GTE:
						possible_optimized = a >= b ? true_data() : false_data();
						break;
					case O_EQ:
						possible_optimized = a == b ? true_data() : false_data();
						break;
					case O_NEQ:
						possible_optimized = a != b ? true_data() : false_data();
						break;
					default:
						can_optimize = false;
						break;
				}
				if (can_optimize) {
					if (!is_boolean(possible_optimized)) {
						// Didn't get optimized to a boolean
						possible_optimized.type = D_NUMBER;
					}
					expression->type = E_LITERAL;
					expression->op.lit_expr = possible_optimized;
					safe_free(left);
					safe_free(right);
					return expression;
				}
			}
			else if (left->type == E_LITERAL && is_boolean(left->op.lit_expr) &&
				right->type == E_LITERAL && is_boolean(right->op.lit_expr)) {
				// Peek Optimization is Available on Booleans
				bool can_optimize = true;
				bool a = left->op.lit_expr.type == D_TRUE;
				bool b = right->op.lit_expr.type == D_TRUE;
				switch (op) {
					case O_AND:
						possible_optimized = a && b ? true_data() : false_data();
						break;
					case O_OR:
						possible_optimized = a || b ? true_data() : false_data();
						break;
					default:
						can_optimize = false;
						break;
				}
				if (can_optimize) {
					expression->type = E_LITERAL;
					expression->op.lit_expr = possible_optimized;
					safe_free(left);
					safe_free(right);
					return expression;
				}
			}
			break;
		}
		case E_UNARY: {
			expression->op.una_expr.operand =
				optimize_expr(expression->op.una_expr.operand);
			enum vm_operator op = expression->op.una_expr.vm_operator;
			struct expr* operand = expression->op.una_expr.operand;
			if (op == O_NEG && operand->type == E_LITERAL &&
				operand->op.lit_expr.type == D_NUMBER) {
				// Apply here
				operand->op.lit_expr.value.number *= -1;
				safe_free(expression);
				return operand;
			}
			if (op == O_NOT && operand->type == E_LITERAL &&
				(operand->op.lit_expr.type == D_TRUE ||
				operand->op.lit_expr.type == D_FALSE)) {
				// Apply here
				operand->op.lit_expr =
					operand->op.lit_expr.type == D_TRUE ? false_data() : true_data();
				safe_free(expression);
				return operand;
			}
			break;
		}
		case E_IF: {
			expression->op.if_expr.condition =
				optimize_expr(expression->op.if_expr.condition);
			expression->op.if_expr.expr_true =
				optimize_expr(expression->op.if_expr.expr_true);
			expression->op.if_expr.expr_false =
				optimize_expr(expression->op.if_expr.expr_false);
			break;
		}
		case E_CALL: {
			expression->op.call_expr.function =
				optimize_expr(expression->op.call_expr.function);
			expression->op.call_expr.arguments =
				optimize_expr_list(expression->op.call_expr.arguments);
			break;
		}
		case E_SUPER_CALL: {
			expression->op.super_call_expr.arguments =
				optimize_expr_list(expression->op.super_call_expr.arguments);
			break;
		}
		case E_LIST: {
			expression->op.list_expr.contents =
				optimize_expr_list(expression->op.list_expr.contents);
			break;
		}
		case E_FUNCTION: {
			expression->op.func_expr.body =
				optimize_statement(expression->op.func_expr.body);
			break;
		}
		case E_TABLE: {
			expression->op.table_expr.values =
				optimize_expr_list(expression->op.table_expr.values);
			break;
		}
		case E_ASSIGN: {
			if (expression->op.assign_expr.lvalue->type == E_LITERAL &&
				expression->op.assign_expr.lvalue->op.lit_expr.type == D_IDENTIFIER) {
					// Only optimize if not lists
				if (get_usage(expression->op.assign_expr.lvalue->op.lit_expr.value.string,
						expression->line, expression->col) == 0) {
					traverse_expr(expression, &optimize_safe_free_impl);
					return 0;
				}
				expression->op.assign_expr.rvalue =
					optimize_expr(expression->op.assign_expr.rvalue);
			}
			break;
		}
	}
	return expression;
}

static struct expr_list* optimize_expr_list(struct expr_list* list) {
	if (!list) return 0;
	list->next = optimize_expr_list(list->next);
	list->elem = optimize_expr(list->elem);
	if (list->elem) {
		return list;
	}
	else {
		// No struct statement
		struct expr_list* next = list->next;
		safe_free(list);
		return next;
	}
}

/* BEGIN SCANNING CODE */
static void scan_statement(struct statement* state) {
	if (!state) return;
	switch (state->type) {
		case S_LET: {
			// Add Usage
			add_node(state->op.let_statement.lvalue,
				state->op.let_statement.rvalue);
			scan_expr(state->op.let_statement.rvalue);
			break;
		}
		case S_OPERATION: {
			enum opcode op = state->op.operation_statement.vm_operator;
			if (op == OP_RET) {
				scan_expr(state->op.operation_statement.operand);
			}
			else if (op == OP_OUTL) {
				scan_expr(state->op.operation_statement.operand);
			}
			else {
				add_modified(state->op.operation_statement.operand->op.lit_expr.value.string,
					state->src_line, 0);
			}
			break;
		}
		case S_BREAK:
		case S_CONTINUE:
		case S_BYTECODE:
			break;
		case S_EXPR: {
			scan_expr(state->op.expr_statement);
			break;
		}
		case S_BLOCK: {
			state->op.block_statement =
				scan_optimize_statement_list(state->op.block_statement);
			break;
		}
		case S_STRUCT: {
			scan_expr(state->op.struct_statement.init_fn);
			scan_expr_list(state->op.struct_statement.instance_members);
			scan_expr_list(state->op.struct_statement.static_members);
			break;
		}
		case S_ENUM: {
			scan_expr_list(state->op.enum_statement.values);
			break;
		}
		case S_IF: {
			scan_expr(state->op.if_statement.condition);
			make_new_block();
			scan_statement(state->op.if_statement.statement_true);
			delete_block();
			make_new_block();
			scan_statement(state->op.if_statement.statement_false);
			delete_block();
			break;
		}
		case S_LOOP: {
			if (state->op.loop_statement.index_var) {
				add_modified(state->op.loop_statement.index_var,
					state->src_line, 0);
			}
			scan_expr(state->op.loop_statement.condition);
			make_new_block();
			scan_statement(state->op.loop_statement.statement_true);
			delete_block();
			break;
		}
		case S_IMPORT: break;
	}
}

static void scan_statement_list(struct statement_list* list) {
	if (!list) return;
	scan_statement(list->elem);
	scan_statement_list(list->next);
}

static void scan_expr(struct expr* expression) {
	if (!expression) return;
	switch (expression->type) {
		case E_LITERAL: {
			if (expression->op.lit_expr.type == D_IDENTIFIER) {
				// Used!
				add_usage(expression->op.lit_expr.value.string,
					expression->line, expression->col);
			}
			break;
		}
		case E_BINARY: {
			if (expression->op.bin_expr.vm_operator == O_MOD_EQUAL) {
				if (expression->op.bin_expr.left->type == E_LITERAL &&
					expression->op.bin_expr.left->op.lit_expr.type == D_IDENTIFIER) {
					add_modified(expression->op.bin_expr.left->op.lit_expr.value.string,
						expression->line, expression->col);
				}
			}
			scan_expr(expression->op.bin_expr.left);
			scan_expr(expression->op.bin_expr.right);
			break;
		}
		case E_UNARY: {
			scan_expr(expression->op.una_expr.operand);
			break;
		}
		case E_IF: {
			scan_expr(expression->op.if_expr.condition);
			scan_expr(expression->op.if_expr.expr_true);
			scan_expr(expression->op.if_expr.expr_false);
			break;
		}
		case E_CALL: {
			scan_expr(expression->op.call_expr.function);
			scan_expr_list(expression->op.call_expr.arguments);
			break;
		}
		case E_SUPER_CALL: {
			scan_expr_list(expression->op.super_call_expr.arguments);
			break;
		}
		case E_LIST: {
			scan_expr_list(expression->op.list_expr.contents);
			break;
		}
		case E_FUNCTION: {
			make_new_block();
			struct expr_list* curr = expression->op.func_expr.parameters;
			while (curr) {
				add_node(curr->elem->op.lit_expr.value.string, 0);
				curr = curr->next;
			}
			scan_statement(expression->op.func_expr.body);
			delete_block();
			break;
		}
		case E_TABLE: {
			scan_expr_list(expression->op.table_expr.values);
			break;
		}
		case E_ASSIGN: {
			if (expression->op.assign_expr.lvalue->type == E_LITERAL &&
				expression->op.assign_expr.lvalue->op.lit_expr.type == D_IDENTIFIER) {
				add_modified(expression->op.assign_expr.lvalue->op.lit_expr.value.string,
					expression->line, expression->col);
			}
			scan_expr(expression->op.assign_expr.lvalue);
			scan_expr(expression->op.assign_expr.rvalue);
			break;
		}
	}
}

static void scan_expr_list(struct expr_list* list) {
	if (!list) return;
	scan_expr(list->elem);
	scan_expr_list(list->next);
}
