#include "global.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// global.c Implements Settings Module and holds Global Constants, as well
//   as safe memory allocation and deallocation!

typedef struct malloc_node {
	char* filename;
	int line_num;
	size_t size;
	void* ptr;
	struct malloc_node* next;
} malloc_node;

static malloc_node* malloc_node_list = 0;
static bool settings_data[SETTINGS_COUNT] = { false };

void set_settings_flag(settings_flags flag) {
	settings_data[flag] = true;
}

bool get_settings_flag(settings_flags flag) {
	return settings_data[flag];
}

void* safe_malloc_impl(size_t size, char* filename, int line_num) {
	malloc_node* new_node = malloc(sizeof(malloc_node));
	new_node->filename = filename;
	new_node->line_num = line_num;
	new_node->size = size;
	new_node->ptr = malloc(size);
	new_node->next = malloc_node_list;
	malloc_node_list = new_node;
	if (!(new_node->ptr)) {
		printf("SafeMalloc: Couldn't allocate memory! %s at line %d.\n", 
			filename, line_num);
	}
	return new_node->ptr;
}

void* safe_calloc_impl(size_t num, size_t size, char* filename, int line_num) {
	malloc_node* new_node = malloc(sizeof(malloc_node));
	new_node->filename = filename;
	new_node->line_num = line_num;
	new_node->size = size;
	new_node->ptr = calloc(num, size);
	new_node->next = malloc_node_list;
	malloc_node_list = new_node;
	if (!(new_node->ptr)) {
		printf("SafeCalloc: Couldn't allocate memory! %s at line %d.\n", 
			filename, line_num);
		safe_exit(1);
	}
	return new_node->ptr;
}

void* safe_realloc_impl(void* ptr, size_t size, char* filename, int line_num) {
	// Traverse Through LL to find the allocated.
	malloc_node* curr = malloc_node_list;
	while (curr) {
		if (curr->ptr == ptr) {
			void* re = realloc(curr->ptr, size);
			if (!re) {
				printf("SafeRealloc: Couldn't reallocate memory! %s at"
					" line %d.\n", filename, line_num);
			}
			curr->ptr = re;
			curr->size = size;
			return re;
		}
		curr = curr->next;
	}
	printf("SafeRealloc: Couldn't find pointer! %s at line %d.\n", filename,
		line_num);
	safe_exit(1);
	return 0;
}

void safe_free_impl(void* ptr, char* filename, int line_num) {
	// Traverse Through LL to find the allocated.
	malloc_node* curr = malloc_node_list;
	malloc_node* prev = 0;
	while (curr) {
		if (curr->ptr == ptr) {
			// Found it
			free(ptr);
			if (prev) {
				prev->next = curr->next;
			}
			else {
				malloc_node_list = curr->next;
			}
			free(curr);
			return;
		}
		prev = curr;
		curr = curr->next;
	}
	printf("SafeFree: Couldn't find pointer! %s at line %d.\n", filename,
		line_num);
	safe_exit(1);
	return;
}

void check_leak() {
	malloc_node* curr = malloc_node_list;
	while (curr) {
		printf("Memory Leak: %zd bytes (%p), allocated from %s at line %d.\n", 
			curr->size, curr->ptr, curr->filename, curr->line_num);
		curr = curr->next;
	}
}

void free_alloc() {
	// Should have an empty list!
	malloc_node* curr = malloc_node_list;
	while (curr) {
		free(curr->ptr);
		malloc_node* tmp = curr;
		curr = curr->next;
		free(tmp);
	}
	return;
}

void safe_exit(int code) {
	free_alloc();
	exit(code);
}
