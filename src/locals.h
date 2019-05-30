#ifndef LOCALS_H
#define LOCALS_H

#include "ast.h"

/*
 * Locals module, assigns local variables to specific slots in the
 *   callstack. This must match closely with the codegen module and
 *   how codegen generates code.
 */

struct statement_list* assign_locals(struct statement_list* ast);


#endif
