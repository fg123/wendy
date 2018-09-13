#include "imports.h"
#include "global.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

import_node* imported_libraries = 0;

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
	imported_libraries = 0;
}

bool has_already_imported_library(char* name) {
	import_node* curr = imported_libraries;
	while (curr) {
		if (streq(curr->name, name)) {
			return true;
		}
		curr = curr->next;
	}
	return false;
}
