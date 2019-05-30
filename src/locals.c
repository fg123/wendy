#include "locals.h"

// TODO(felixguo): This shares a lot of structure with optimizer
//   ideally it should share structure.


// Linked list node of identifiers within a struct statement block
struct id_node {
	char* id_name;
	int offset;
	struct id_node* next;
};

// struct statement scope block to simulate function calls and checking if identifiers exist.
struct statement_block {
	struct id_node* id_list;
	struct statement_block* next;
};

static struct statement_block *current_block = 0;

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

static void add_node(char* id) {
	if (!curr_statement_block) {
		// TODO: locals error here instead
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK);
		return;
	}
	struct id_node* new_node = safe_malloc(sizeof(struct id_node));
	new_node->id_name = id;
	new_node->offset = 0;
	if (curr_statement_block->id_list) {
		new_node->offset = curr_statement_block->id_list->offset + 1;
	}
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

void assign_statement_list(struct statement_list* list,
						   struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	return;
}

void assign_statement(struct statement* state,
					  struct traversal_algorithm *algo) {
	if (state->type == S_LET) {
		// Create a new id_node

	}
	UNUSED(algo);
	return;
}

void assign_expr(struct expr* expression,
				 struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(expression);
	return;
}

void assign_expr_list(struct expr_list* list,
					  struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	return;
}

struct traversal_algorithm assign_algo_impl = {
	assign_expr,
	assign_expr_list,
	assign_statement,
	assign_statement_list,
	HANDLE_BEFORE_CHILDREN, 0
};

struct statement_list* assign_locals(struct statement_list* ast) {
	if (get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
		traverse_statement_list(ast, &assign_algo_impl);
	}
	return ast;
}
