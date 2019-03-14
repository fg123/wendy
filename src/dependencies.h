#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H

#include "ast.h"

// dependencies.h - Felix Guo
// Function that analyzes an AST and prints the dependencies

// print_dependencies(ast) analyzes an AST and prints the dependencies
void print_dependencies(struct statement_list* ast);

#endif
