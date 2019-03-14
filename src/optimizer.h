#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ast.h"

// optimizer.h - Felix Guo
// Includes two different types of optimization algorithms, one to prune down
//   AST and another to optimize bytecode instructions/

struct statement_list* optimize_ast(struct statement_list* ast);

#endif
