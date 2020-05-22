#include "locals.h"
#include "error.h"

// Linked list node of identifiers within a struct statement block
struct id_node {
	char* id_name;
	int offset;
	struct id_node* next;
};

// struct statement scope block to simulate function calls and checking if identifiers exist.
struct statement_block {
	struct id_node* id_list;
	bool closure_boundary;
	int offset;
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

static void debug_print(void) {
	fprintf(stderr, "DEBUG PRINT ===\n");
	struct statement_block* curr = curr_statement_block;
	while (curr) {
		fprintf(stderr, "Block (%p): \n", curr);
		struct id_node* list = curr->id_list;
		while (list) {
			fprintf(stderr, "\t%s: %d\n", list->id_name, list->offset);
			list = list->next;
		}
		curr = curr->next;
	}
	fprintf(stderr, "===============\n");
}

static struct id_node* add_node(char* id) {
	if (!curr_statement_block) {
		// TODO: locals error here instead
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK ": %s", id);
		return NULL;
	}
	struct id_node* new_node = safe_malloc(sizeof(struct id_node));
	new_node->id_name = id;
	new_node->offset = curr_statement_block->offset;
	if (curr_statement_block->id_list) {
		new_node->offset = curr_statement_block->id_list->offset + 1;
	}
	new_node->next = curr_statement_block->id_list;
	curr_statement_block->id_list = new_node;
	return new_node;
}

struct offset_return {
	bool is_global;
	int offset;
};

static struct offset_return get_offset_of_identifier(char* id, int line, int col) {
	// TODO(felixguo): Do Closures
	if (!curr_statement_block) {
		// TODO: locals error here instead
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK ": %s", id);
		return (struct offset_return) { false, -1 };
	}
	if (streq(id, "this")) {
		return (struct offset_return) { false, -1 };
	}
	struct statement_block* curr_block = curr_statement_block;
	while (curr_block != global_statement_block) {
		struct id_node* curr = curr_block->id_list;
		while (curr) {
			if (streq(curr->id_name, id)) {
				return (struct offset_return){ false, curr->offset };
			}
			curr = curr->next;
		}
		if (curr_block->closure_boundary) break;
		curr_block = curr_block->next;
	}
	// Search Global Too
	struct id_node* curr = global_statement_block->id_list;
	while (curr) {
		if (streq(curr->id_name, id)) {
			return (struct offset_return){ true, curr->offset };
		}
		curr = curr->next;
	}
	// TODO: make error message
	error_lexer(line, col, MEMORY_ID_NOT_FOUND, id);
	return (struct offset_return){ false, -1 };
}

static void make_new_block(bool closure_boundary) {
	fprintf(stderr, "Make new block\n");
	struct statement_block* new_block = safe_malloc(sizeof(struct statement_block));
	new_block->next = curr_statement_block;
	new_block->id_list = 0;
	new_block->closure_boundary = closure_boundary;

	if (!closure_boundary && curr_statement_block) {
		new_block->offset = curr_statement_block->offset;
	}
	else {
		new_block->offset = 2;
	}

	if (!curr_statement_block) {
		// First Ever is the Global Block
		global_statement_block = new_block;
	}
	curr_statement_block = new_block;

	debug_print();
}

static void delete_block(void) {
	fprintf(stderr, "Delete block\n");
	if (curr_statement_block) {
		struct statement_block* next = curr_statement_block->next;
		free_statement_block(curr_statement_block);
		curr_statement_block = next;
	}
	debug_print();
}

void assign_statement_list_pre(struct statement_list* list,
						       struct traversal_algorithm *algo) {
	UNUSED(algo);
	UNUSED(list);
	if (list->is_first) make_new_block(false);
	return;
}

void assign_statement_pre(struct statement* state,
					      struct traversal_algorithm *algo) {
	if (state->type == S_LET) {
		// Create a new id_node
		struct id_node* node = add_node(state->op.let_statement.lvalue);
		state->op.let_statement.lvalue_offset = make_data(
				curr_statement_block == global_statement_block ? D_IDENTIFIER_GLOBAL_OFFSET : D_IDENTIFIER_LOCAL_OFFSET,
				data_value_num(node->offset));
	}
	else if (state->type == S_LOOP) {
		make_new_block(false);
		if (state->op.loop_statement.index_var) {
			// Add 3 slots for list usage
			add_node(":::1");
			add_node(":::2");
			add_node(":::3");
			state->op.loop_statement.loop_var_offset = get_offset_of_identifier(":::1", 0, 0).offset;
			add_node(state->op.loop_statement.index_var);
		}
	}
	UNUSED(algo);
	return;
}

void assign_expr_pre(struct expr* expression,
				     struct traversal_algorithm *algo) {
	UNUSED(algo);
	if (expression->type == E_LITERAL) {
		if (expression->op.lit_expr.type == D_IDENTIFIER) {
			char* id = expression->op.lit_expr.value.string;
			struct offset_return oret = get_offset_of_identifier(id,
				expression->line, expression->col);
			if (oret.offset < 0) {
				return;
			}
			destroy_data(&expression->op.lit_expr);
			expression->op.lit_expr = make_data(
				oret.is_global ? D_IDENTIFIER_GLOBAL_OFFSET : D_IDENTIFIER_LOCAL_OFFSET,
				data_value_num(oret.offset));
		}
	}
	else if (expression->type == E_FUNCTION) {
		// Make collapsible extra block to store parameters
		make_new_block(true);
		// Add 2 because codegen pushes self and fn and arguments
		curr_statement_block->offset += 3;
		struct expr_list* param_list = expression->op.func_expr.parameters;
		while (param_list) {
			struct expr* e = param_list->elem;
			if (e->type == E_LITERAL) {
				add_node(e->op.lit_expr.value.string);
			}
			else {
				add_node(e->op.assign_expr.lvalue->op.lit_expr.value.string);
			}
			param_list = param_list->next;
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
	if (list->is_last) delete_block();
	return;
}

void assign_statement_post(struct statement* state,
					       struct traversal_algorithm *algo) {
	UNUSED(algo);
	if (state->type == S_LOOP) {
		delete_block();
	}
	return;
}

void assign_expr_post(struct expr* expression,
				      struct traversal_algorithm *algo) {
	UNUSED(algo);
	if (expression->type == E_FUNCTION) {
		delete_block();
	}
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
