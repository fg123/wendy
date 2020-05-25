#include "locals.h"
#include "error.h"

// Linked list node of identifiers within a struct statement block
struct id_node {
	char* id_name;
	int offset;

	// offset of this variable in the parent frame
	int closure_offset;

	struct id_node* next;
};

struct statement_block {
	struct id_node* id_list;
	bool is_closure_boundary;
	int offset;
	struct statement_block* next;
	int id_count;

	// Only the head_block / closure_boundary should have this
	struct closure_mapping* closure;
	struct statement_block* head_block;
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
		fprintf(stderr, "Block (%p) (%d) (%s): \n", curr, curr->offset, curr->is_closure_boundary ? "true" : "false");
		struct id_node* list = curr->id_list;
		while (list) {
			fprintf(stderr, "\t%s: %d (%d)\n", list->id_name, list->offset, list->closure_offset);
			list = list->next;
		}
		curr = curr->next;
	}
	fprintf(stderr, "===============\n");
}

static struct id_node* add_node(struct statement_block* block, char* id, int closure_offset) {
	if (!block) {
		// TODO: locals error here instead
		error_general(OPTIMIZER_NO_STATEMENT_BLOCK ": %s", id);
		return NULL;
	}
	struct id_node* new_node = safe_malloc(sizeof(struct id_node));
	new_node->id_name = id;
	new_node->offset = block->offset;
	new_node->closure_offset = closure_offset;

	block->offset += 1;
	block->id_count += 1;

	new_node->next = block->id_list;
	block->id_list = new_node;

	fprintf(stderr, "Add node %s\n", id);
	debug_print();
	return new_node;
}

struct offset_return {
	bool is_global;
	int offset;
};

static struct offset_return get_offset_of_identifier(char* id, int line, int col) {
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
		if (curr_block->is_closure_boundary) break;
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

	if (curr_block->is_closure_boundary) {
		printf("Can't find %s in local or global, looking in closures...\n", id);
		struct id_node* found_name = 0;
		struct statement_block* closure_points[1000];
		int closure_point_count = 0;

		closure_points[closure_point_count++] = curr_statement_block;

		bool just_passed_boundary = false;
		// Add every block that's right before a closure boundary until
		//   we find the identifier we're looking for.
		while (curr_block != global_statement_block) {
			if (just_passed_boundary) {
				closure_points[closure_point_count++] = curr_block;
			}

			just_passed_boundary = curr_block->is_closure_boundary;

			struct id_node* curr = curr_block->id_list;
			while (curr) {
				if (streq(curr->id_name, id)) {
					found_name = curr;
					break;
				}
				curr = curr->next;
			}
			if (found_name) break;
			curr_block = curr_block->next;
		}
		if (!found_name) {
			// TODO: make error message
			error_lexer(line, col, MEMORY_ID_NOT_FOUND, id);
			return (struct offset_return){ false, -1 };
		}
		// curr_block stores the block where the id resides, traverse
		//   back down to add closure nodes.
		printf("Found id in block %p\n", curr_block);
		int offset = found_name->offset;
		// Traverse backwards starting from the frame before
		//   we found the ID
		for (int i = closure_point_count - 2; i >= 0; i--) {
			add_node(closure_points[i], id, offset);
			if (!closure_points[i]->head_block) {
				error_general("What the heck is this? No head block!?");
				safe_exit(1);
			}
			struct closure_mapping* new_mapping = safe_malloc(sizeof(struct closure_mapping));
			new_mapping->next = closure_points[i]->head_block->closure;
			new_mapping->parent_offset = offset;
			new_mapping->offset = closure_points[i]->offset - 1;
			closure_points[i]->head_block->closure = new_mapping;
			offset = closure_points[i]->offset - 1;
		}
		return (struct offset_return){ false, offset };
	}
	error_lexer(line, col, MEMORY_ID_NOT_FOUND, id);
	return (struct offset_return){ false, -1 };
}

static void make_new_block(bool closure_boundary) {
	fprintf(stderr, "Make new block\n");
	struct statement_block* new_block = safe_malloc(sizeof(struct statement_block));
	new_block->next = curr_statement_block;
	new_block->id_list = 0;
	new_block->is_closure_boundary = closure_boundary;
	new_block->closure = 0;
	new_block->id_count = 0;

	if (closure_boundary) {
		new_block->head_block = new_block;
	}
	else if (curr_statement_block) {
		new_block->head_block = curr_statement_block->head_block;
	}
	else {
		new_block->head_block = 0;
	}

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
	// We leave the global one intact so we can pull global symbols
	if (curr_statement_block && curr_statement_block != global_statement_block) {
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
	UNUSED(state);
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
		// The Function Stack Frame looks like:
		// FP-> [FunctionXX]
		//      [RETURN VALUE]
		//      [self -> function<()>] Offset 2
		//      [arguments -> []] Offset 4
		//      [params] Offset ...
		//      [closure variables] Offset ...
		curr_statement_block->offset = 2;
		add_node(curr_statement_block, "self", 0);
		// Bind the function name to the same offset as the self bind
		if (expression->op.func_expr.bind_name) {
			curr_statement_block->offset -= 1;
			add_node(curr_statement_block, expression->op.func_expr.bind_name, 0);
		}

		add_node(curr_statement_block, "arguments", 0);

		struct expr_list* param_list = expression->op.func_expr.parameters;
		while (param_list) {
			struct expr* e = param_list->elem;
			if (e->type == E_LITERAL) {
				add_node(curr_statement_block, e->op.lit_expr.value.string, 0);
			}
			else {
				add_node(curr_statement_block, e->op.assign_expr.lvalue->op.lit_expr.value.string, 0);
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
	UNUSED(state);
	if (state->type == S_LET) {
		// Create a new id_node, after we processed the LET statement
		struct id_node* node = add_node(curr_statement_block,
			state->op.let_statement.lvalue, 0);
		state->op.let_statement.lvalue_offset = make_data(
				curr_statement_block == global_statement_block ? D_IDENTIFIER_GLOBAL_OFFSET : D_IDENTIFIER_LOCAL_OFFSET,
				data_value_num(node->offset));
	}
	return;
}

void assign_expr_post(struct expr* expression,
				      struct traversal_algorithm *algo) {
	UNUSED(algo);
	if (expression->type == E_FUNCTION) {
		if (!curr_statement_block->is_closure_boundary) {
			error_general("What the heck? This should be a closure boundary!");
		}
		expression->op.func_expr.closure = curr_statement_block->closure;
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

struct statement_list* assign_locals(struct statement_list* ast, id_list* list) {
	if (get_settings_flag(SETTINGS_OPTIMIZE_LOCALS)) {
		traverse_statement_list(ast, &assign_algo_impl);
		if (curr_statement_block != global_statement_block) {
			error_general("End of assign locals but still have a scope block remaining!");
		}
		if (global_statement_block) {
			*list = safe_malloc(sizeof(char*) * (global_statement_block->id_count + 1));
			size_t i = 0;
			struct id_node* node = global_statement_block->id_list;
			while (node) {
				*list[i++] = safe_strdup(node->id_name);
				node = node->next;
			}
			*list[i] = 0;

		}
		else {
			*list = 0;
		}
		free_statement_block(global_statement_block);
	}
	return ast;
}
