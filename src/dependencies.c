#include "dependencies.h"
#include <stdio.h>


static void do_nothing(void* c) { }

static void handle_statement(void* v) {
	statement* s = (statement*) v;
	if (s->type == S_IMPORT) {
		// Has Dependency
		printf("%s\n", s->op.import_statement.t_data.string);
	}
}

void print_dependencies(statement_list* ast) {
	// expr, expr_list, statement, statement_list
	traverse_statement_list(ast, do_nothing, do_nothing, 
		handle_statement, do_nothing);
}

