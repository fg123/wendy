#include "locals.h"
#include "error.h"

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

static struct statement_block* curr_statement_block = 0;
static struct statement_block* global_statement_block = 0;

static void free_id_nodes(struct id_node* node);

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

struct offset_return {
	bool is_global;
	int offset;
};

static struct offset_return get_offset_of_identifier(char* id) {
	// TODO(felixguo): Do Closures
	if (!curr_statement_block) {
		// TODO: locals error here instead
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK);
		return;
	}
	struct id_node* curr = curr_statement_block->id_list;
	while (curr) {
		if (streq(curr->id_name, id)) {
			return { false, curr->offset };
		}
		curr = curr->next;
	}
	// Search Global Too
	curr = global_statement_block->id_list;
	while (curr) {
		if (streq(curr->id_name, id)) {
			return { true, curr->offset };
		}
		curr = curr->next;
	}
	return { false, -1 };
}

static void make_new_block(void) {
	struct statement_block* new_block = safe_malloc(sizeof(struct statement_block));
	new_block->next = curr_statement_block;
	new_block->id_list = 0;
	if (!curr_statement_block) {
		// First Ever is the Global Block
		global_statement_block = new_block;
	}
	curr_statement_block = new_block;
}

static void delete_block(void) {
	struct statement_block* next = curr_statement_block->next;
	free_statement_block(curr_statement_block);
	curr_statement_block = next;
}

void assign_statement_list_pre(struct statement_list* list,
						       struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	return;
}

void assign_statement_pre(struct statement* state,
					      struct traversal_algorithm *algo) {
	if (state->type == S_LET) {
		// Create a new id_node
		add_node(state->op.let_statement.lvalue);
	}
	UNUSED(algo);
	return;
}

void assign_expr_pre(struct expr* expression,
				     struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(expression);
	if (expression->type == E_LITERAL) {
		if (expression->op.lit_expr.type == D_IDENTIFIER) {
			char* id = expression=>op.lit_expr.value.string;
			struct offset_return oret = get_offset_of_identifier(id);
			if (oret.offset < 0) {
				error_runtime(expression->line, MEMORY_ID_NOT_FOUND, id);
				return;
			}
			destroy_data(&expression->op.lit_expr);
			expression->op.lit_expr = make_data(
				oret.is_global ? D_IDENTIFIER_GLOBAL_OFFSET : D_IDENTIFIER_LOCAL_OFFSET,
				data_value_num(oret.offset));
		}
	}
	return;
}

void assign_expr_list_pre(struct expr_list* list,
					      struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	return;
}

void assign_statement_list_post(struct statement_list* list,
						        struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	make_new_block();
	return;
}

void assign_statement_post(struct statement* state,
					       struct traversal_algorithm *algo) {
	UNUSED(state);
	UNUSED(algo);
	return;
}

void assign_expr_post(struct expr* expression,
				      struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(expression);
	delete_block();
	return;
}

void assign_expr_list_post(struct expr_list* list,
		     			  struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	return;
}

struct traversal_algorithm assign_algo_impl = TRAVERSAL_ALGO(
	assign_expr_pre,
	assign_expr_list_pre,
	assign_statement_pre,
	assign_statement_list_pre,
	assign_expr_post,
	assign_expr_list_post,
	assign_statement_post,
	assign_statement_list_post
);

struct statement_list* assign_locals(struct statement_list* ast) {
	if (get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
		traverse_statement_list(ast, &assign_algo_impl);
	}
	return ast;
}
