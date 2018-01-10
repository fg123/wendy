#include "dependencies.h"
#include <stdio.h>

static void do_nothing_e(expr* e, traversal_algorithm* algo) { UNUSED(e); UNUSED(algo); }
static void do_nothing_el(expr_list* e, traversal_algorithm* algo) { UNUSED(e); UNUSED(algo); }
static void do_nothing_sl(statement_list* e, traversal_algorithm* algo) { UNUSED(e); UNUSED(algo); }
static void handle_statement(statement* s, traversal_algorithm* algo) {
	UNUSED(algo);
	if (s->type == S_IMPORT) {
		// Has Dependency
		printf("%s\n", s->op.import_statement.t_data.string);
	}
}

traversal_algorithm dependency_print_impl = {
	do_nothing_e, 
	do_nothing_el,
	handle_statement,
	do_nothing_sl,
	HANDLE_BEFORE_CHILDREN,
	0
};

void print_dependencies(statement_list* ast) {
	traverse_ast(ast, &dependency_print_impl);
}

