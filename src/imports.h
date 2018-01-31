#ifndef IMPORTS_H
#define IMPORTS_H

#include <stdbool.h>

// imports.h - Felix Guo
// Tracks a linked list of imported libraries both during codegen
//   and VM execution

void init_imported_libraries_ll(void);
void add_imported_library(char *name);
void free_imported_libraries_ll(void);
bool has_already_imported_library(char *name);

#endif
