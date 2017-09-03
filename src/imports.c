#include "imports.h"
#include <stdbool.h>

typedef struct import_node {
    char* name;
    struct import_node* next;
} import_node;

import_node* imported_libraries = 0;

void init_imported_libraries_ll() {
	imported_libraries = 0;
}

void add_imported_library(char* name) {
    import_node* new_node = safe_malloc(sizeof(import_node));
    new_node->name = safe_malloc(sizeof(char) * (strlen(name) + 1));
    strcpy(new_node->name, name);
    new_node->next = imported_libraries;
    imported_libraries = new_node;
} 

void free_imported_libraries_ll() {
    import_node* curr = imported_libraries;
    while (curr) {
        safe_free(curr->name);
        import_node* next = curr->next;
        safe_free(curr);
        curr = next;
    }
}

bool has_already_imported_library(char* name) {
    import_node* curr = imported_libraries;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}
