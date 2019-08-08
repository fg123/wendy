#include "dependencies.h"
#include <stdio.h>

static void do_nothing_e(struct expr* e, struct traversal_algorithm* algo) { UNUSED(e); UNUSED(algo); }
static void do_nothing_el(struct expr_list* e, struct traversal_algorithm* algo) { UNUSED(e); UNUSED(algo); }
static void do_nothing_sl(struct statement_list* e, struct traversal_algorithm* algo) { UNUSED(e); UNUSED(algo); }
static void handle_statement(struct statement* s, struct traversal_algorithm* algo) {
	UNUSED(algo);
	if (s->type == S_IMPORT) {
		// Log Dependency
		printf("%s\n", s->op.import_statement);
	}
}

struct traversal_algorithm dependency_print_impl =
	TRAVERSAL_ALGO_PRE(do_nothing_e, do_nothing_el,
		handle_statement, do_nothing_sl);

void print_dependencies(struct statement_list* ast) {
	traverse_ast(ast, &dependency_print_impl);
}

