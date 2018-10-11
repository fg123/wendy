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

// Linked list node of identifiers within a statement block
typedef struct id_node id_node;
struct id_node {
	char* id_name;
	expr* value;
	int modified_count;
	int usage_count;
	id_node* next;
};

// Statement scope block to simulate function calls and checking if identifiers exist.
typedef struct statement_block statement_block;
struct statement_block {
	id_node* id_list;
	statement_block* next;
};

static statement_block* curr_statement_block = 0;

// Forward Declarations
static void free_id_nodes(id_node* node);
static void free_statement_blocks(statement_block* block);
static statement_list* optimize_statement_list(statement_list* list);
static statement* optimize_statement(statement* state);
static expr* optimize_expr(expr* expression);
static expr_list* optimize_expr_list(expr_list* list);
static statement_list* scan_statement_list_with_new_block(statement_list* list);
static void scan_statement_list(statement_list* list);
static void scan_statement(statement* state);
static void scan_expr(expr* expression);
static void scan_expr_list(expr_list* list);

static inline bool is_boolean(data t) {
	return t.type == D_TRUE || t.type == D_FALSE;
}

static id_node* find_id_node(char* id, int line, int col) {
	UNUSED(line);
	UNUSED(col);
	// Search in each block and each list. Returns null if not found for now.
	// TODO: perform identifier checking here, but will need to add in
	//   special cases like `this` for OOP
	statement_block* b = curr_statement_block;
	while (b) {
		id_node* n = b->id_list;
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
	statement_block* b = curr_statement_block;
	while (b) {
		id_node** n = &b->id_list;
		while (*n) {
			if (streq((*n)->id_name, id)) {
				id_node* next = (*n)->next;
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
	id_node* r = find_id_node(id, line, col);
	if (r) r->usage_count++;
}

static void remove_usage(char* id, int line, int col) {
	id_node* r = find_id_node(id, line, col);
	if (r) r->usage_count--;
}

static int get_usage(char* id, int line, int col) {
	id_node* r = find_id_node(id, line, col);
	if (r) return r->usage_count;
	else return 100;
}

static expr* get_value(char* id, int line, int col) {
	id_node* r = find_id_node(id, line, col);
	if (r) return r->value;
	else return 0;
}

static int get_modified(char* id, int line, int col) {
	id_node* r = find_id_node(id, line, col);
	if (r) return r->modified_count;
	else return 100;
}

static void add_modified(char* id, int line, int col) {
	id_node* r = find_id_node(id, line, col);
	if (r) r->modified_count++;
}

void optimize_safe_free_e(expr* expression, traversal_algorithm* algo) {
	UNUSED(algo);
	if (expression->type == E_LITERAL
		&& expression->op.lit_expr.type == D_IDENTIFIER) {
		remove_usage(expression->op.lit_expr.value.string,
				expression->line, expression->col);
	}
    ast_safe_free_e(expression, algo);
}
void optimize_safe_free_el(expr_list* ptr, traversal_algorithm* algo) {
    UNUSED(algo);
    ast_safe_free_el(ptr, algo);
}

void optimize_safe_free_s(statement* ptr, traversal_algorithm* algo) {
    UNUSED(algo);
    ast_safe_free_s(ptr, algo);
}

void optimize_safe_free_sl(statement_list* ptr, traversal_algorithm* algo) {
    UNUSED(algo);
    ast_safe_free_sl(ptr, algo);
}

traversal_algorithm optimize_safe_free_impl = {
	optimize_safe_free_e,
	optimize_safe_free_el,
	optimize_safe_free_s,
	optimize_safe_free_sl,
	HANDLE_AFTER_CHILDREN, 0
};

statement_list* optimize_ast(statement_list* ast) {
	statement_list* result = scan_statement_list_with_new_block(ast);
	free_statement_blocks(curr_statement_block);
	return result;
}

static void free_statement_block(statement_block* block) {
	if (!block) return;
	free_id_nodes(curr_statement_block->id_list);
	safe_free(block);
}

void free_statement_blocks(statement_block* block) {
	if (!block) return;
	free_statement_blocks(block->next);
	free_statement_block(block);
}

static void free_id_nodes(id_node* node) {
	if (!node) return;
	free_id_nodes(node->next);
	safe_free(node);
}

static void add_node(char* id, expr* value) {
	if (!curr_statement_block) {
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK);
	}
	id_node* new_node = safe_malloc(sizeof(id_node));
	new_node->id_name = id;
	new_node->value = value;
	new_node->modified_count = 0;
	new_node->usage_count = 0;
	new_node->next = curr_statement_block->id_list;
	curr_statement_block->id_list = new_node;
}

static void make_new_block(void) {
	statement_block* new_block = safe_malloc(sizeof(statement_block));
	new_block->next = curr_statement_block;
	new_block->id_list = 0;
	curr_statement_block = new_block;
}

static void delete_block(void) {
	statement_block* next = curr_statement_block->next;
	free_statement_block(curr_statement_block);
	curr_statement_block = next;
}

static statement_list* scan_optimize_statement_list(statement_list* list) {
	// We run two passes of optimization for some unknown reason. I added this
	//   back in the day and I can't remember why :)
	scan_statement_list(list);
	list = optimize_statement_list(list);
	list = optimize_statement_list(list);
	return list;
}
static statement_list* scan_statement_list_with_new_block(statement_list* list) {
	make_new_block();
	list = scan_optimize_statement_list(list);
	delete_block();
	return list;
}

/* OPTIMIZE CODE */
static statement* optimize_statement(statement* state) {
	if (!state) return 0;
	if (state->type == S_LET) {
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
	}
	else if (state->type == S_OPERATION) {

	}
	else if (state->type == S_EXPR) {
		state->op.expr_statement = optimize_expr(state->op.expr_statement);
	}
	else if (state->type == S_BLOCK) {
		if (!state->op.block_statement) {
			return 0;
		}
	}
	else if (state->type == S_STRUCT) {
		state->op.struct_statement.init_fn =
			 optimize_expr(state->op.struct_statement.init_fn);
		state->op.struct_statement.instance_members =
			optimize_expr_list(state->op.struct_statement.instance_members);
		state->op.struct_statement.static_members =
			optimize_expr_list(state->op.struct_statement.static_members);
	}
	else if (state->type == S_IF) {
		state->op.if_statement.condition =
			optimize_expr(state->op.if_statement.condition);
		expr* condition = state->op.if_statement.condition;

		state->op.if_statement.statement_true =
			optimize_statement(state->op.if_statement.statement_true);
		state->op.if_statement.statement_false =
			optimize_statement(state->op.if_statement.statement_false);

		statement* run_if_false = state->op.if_statement.statement_false;
		statement* run_if_true = state->op.if_statement.statement_true;
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
	}
	else if (state->type == S_LOOP) {
		state->op.loop_statement.condition =
			optimize_expr(state->op.loop_statement.condition);
		state->op.loop_statement.statement_true =
			optimize_statement(state->op.loop_statement.statement_true);
		if (!state->op.loop_statement.statement_true) {
			// Empty Body
			traverse_expr(state->op.loop_statement.condition, &optimize_safe_free_impl);
			return 0;
		}
	}
	return state;
}

static statement_list* optimize_statement_list(statement_list* list) {
	if (!list) return 0;
	list->next = optimize_statement_list(list->next);
	list->elem = optimize_statement(list->elem);
	if (list->elem) {
		return list;
	}
	else {
		// No Statement
		statement_list* next = list->next;
		safe_free(list);
		return next;
	}
}

static expr* optimize_expr(expr* expression) {
	if (!expression) return 0;
	if (expression->type == E_LITERAL) {
		if (expression->op.lit_expr.type == D_IDENTIFIER) {
			// Is a static, then replace with value if literal
			if (get_modified(expression->op.lit_expr.value.string,
				expression->line, expression->col) == 0) {
				expr* value = get_value(expression->op.lit_expr.value.string,
					expression->line, expression->col);
				if (value && value->type == E_LITERAL) {
					remove_usage(expression->op.lit_expr.value.string,
						expression->line, expression->col);
                    destroy_data(&expression->op.lit_expr);
					*expression = *value;
				}
			}
		}
	}
	else if (expression->type == E_BINARY) {
		expression->op.bin_expr.left =
			optimize_expr(expression->op.bin_expr.left);
		expression->op.bin_expr.right =
			optimize_expr(expression->op.bin_expr.right);

		expr* left = expression->op.bin_expr.left;
		expr* right = expression->op.bin_expr.right;
		enum operator op = expression->op.bin_expr.operator;
        data possible_optimized;
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
	}
	else if (expression->type == E_UNARY) {
		expression->op.una_expr.operand =
			optimize_expr(expression->op.una_expr.operand);
		enum operator op = expression->op.una_expr.operator;
		expr* operand = expression->op.una_expr.operand;
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
	}
	else if (expression->type == E_IF) {
		expression->op.if_expr.condition =
			optimize_expr(expression->op.if_expr.condition);
		expression->op.if_expr.expr_true =
			optimize_expr(expression->op.if_expr.expr_true);
		expression->op.if_expr.expr_false =
			optimize_expr(expression->op.if_expr.expr_false);
	}
	else if (expression->type == E_CALL) {
		expression->op.call_expr.function =
			optimize_expr(expression->op.call_expr.function);
		expression->op.call_expr.arguments =
			optimize_expr_list(expression->op.call_expr.arguments);
	}
	else if (expression->type == E_LIST) {
		expression->op.list_expr.contents =
			optimize_expr_list(expression->op.list_expr.contents);
	}
	else if (expression->type == E_FUNCTION) {
		expression->op.func_expr.body =
			optimize_statement(expression->op.func_expr.body);
	}
	else if (expression->type == E_ASSIGN) {
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
	}
	return expression;
}

static expr_list* optimize_expr_list(expr_list* list) {
	if (!list) return 0;
	list->next = optimize_expr_list(list->next);
	list->elem = optimize_expr(list->elem);
	if (list->elem) {
		return list;
	}
	else {
		// No Statement
		expr_list* next = list->next;
		safe_free(list);
		return next;
	}
}

/* BEGIN SCANNING CODE */
static void scan_statement(statement* state) {
	if (!state) return;
	if (state->type == S_LET) {
		// Add Usage
		add_node(state->op.let_statement.lvalue,
			state->op.let_statement.rvalue);
		scan_expr(state->op.let_statement.rvalue);
	}
	else if (state->type == S_OPERATION) {
		opcode op = state->op.operation_statement.operator;
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
	}
	else if (state->type == S_EXPR) {
		scan_expr(state->op.expr_statement);
	}
	else if (state->type == S_BLOCK) {
		state->op.block_statement =
			scan_optimize_statement_list(state->op.block_statement);
	}
	else if (state->type == S_STRUCT) {
		scan_expr(state->op.struct_statement.init_fn);
		scan_expr_list(state->op.struct_statement.instance_members);
		scan_expr_list(state->op.struct_statement.static_members);
	}
	else if (state->type == S_IF) {
		scan_expr(state->op.if_statement.condition);
		make_new_block();
		scan_statement(state->op.if_statement.statement_true);
		delete_block();
		make_new_block();
		scan_statement(state->op.if_statement.statement_false);
		delete_block();
	}
	else if (state->type == S_LOOP) {
		scan_expr(state->op.loop_statement.condition);
		make_new_block();
		scan_statement(state->op.loop_statement.statement_true);
		delete_block();
	}
}

static void scan_statement_list(statement_list* list) {
	if (!list) return;
	scan_statement(list->elem);
	scan_statement_list(list->next);
}

static void scan_expr(expr* expression) {
	if (!expression) return;
	if (expression->type == E_LITERAL) {
		if (expression->op.lit_expr.type == D_IDENTIFIER) {
			// Used!
			add_usage(expression->op.lit_expr.value.string,
				expression->line, expression->col);
		}
	}
	else if (expression->type == E_BINARY) {
		scan_expr(expression->op.bin_expr.left);
		scan_expr(expression->op.bin_expr.right);
	}
	else if (expression->type == E_UNARY) {
		scan_expr(expression->op.una_expr.operand);
	}
	else if (expression->type == E_IF) {
		scan_expr(expression->op.if_expr.condition);
		scan_expr(expression->op.if_expr.expr_true);
		scan_expr(expression->op.if_expr.expr_false);
	}
	else if (expression->type == E_CALL) {
		scan_expr(expression->op.call_expr.function);
		scan_expr_list(expression->op.call_expr.arguments);
	}
	else if (expression->type == E_LIST) {
		scan_expr_list(expression->op.list_expr.contents);
	}
	else if (expression->type == E_FUNCTION) {
		make_new_block();
		expr_list* curr = expression->op.func_expr.parameters;
		while (curr) {
			add_node(curr->elem->op.lit_expr.value.string, 0);
			curr = curr->next;
		}
		scan_statement(expression->op.func_expr.body);
		delete_block();
	}
	else if (expression->type == E_ASSIGN) {
		if (expression->op.assign_expr.lvalue->type == E_LITERAL &&
			expression->op.assign_expr.lvalue->op.lit_expr.type == D_IDENTIFIER) {
			add_modified(expression->op.assign_expr.lvalue->op.lit_expr.value.string,
				expression->line, expression->col);
		}
		scan_expr(expression->op.assign_expr.lvalue);
		scan_expr(expression->op.assign_expr.rvalue);
	}
}

static void scan_expr_list(expr_list* list) {
	if (!list) return;
	scan_expr(list->elem);
	scan_expr_list(list->next);
}
