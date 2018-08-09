#ifndef IMPORTS_H
#define IMPORTS_H

#include <stdbool.h>

// imports.h - Felix Guo
// Tracks a linked list of imported libraries both during codegen
//   and VM execution

typedef struct import_node {
    char* name;
    struct import_node* next;
} import_node;

extern import_node* imported_libraries;

void add_imported_library(char *name);
void free_imported_libraries_ll(void);
bool has_already_imported_library(char *name);

#endif
