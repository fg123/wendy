#include "global.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct malloc_node {
	char* filename;
	int line_num;
	size_t size;
	void* ptr;
	struct malloc_node* next;
	struct malloc_node* prev;
} malloc_node;

static malloc_node* malloc_node_start = 0;
static malloc_node* malloc_node_end = 0;
static bool settings_data[SETTINGS_COUNT] = { false };
bool is_big_endian = true;
bool last_printed_newline = false;

char* safe_strdup_impl(const char* s, char* allocated) {
	strcpy(allocated, s);
	return allocated;
}

inline bool streq(const char* a, const char* b) {
	return strcmp(a, b) == 0;
}

void determine_endianness() {
	int i = 1;
	is_big_endian = ((*(char*)&i) == 0);
}

void set_settings_flag(settings_flags flag) {
	settings_data[flag] = true;
}

bool get_settings_flag(settings_flags flag) {
	return settings_data[flag];
}

static void attach_to_list(malloc_node* new_node) {
	if (!malloc_node_end && !malloc_node_start) {
		malloc_node_start = new_node;
		malloc_node_end = new_node;
	}

	malloc_node_end->next = new_node;
	new_node->prev = malloc_node_end;

	malloc_node_start->prev = new_node;
	new_node->next = malloc_node_start;

	malloc_node_end = new_node;
}

static void remove_from_list(malloc_node* node) {
	if (node == malloc_node_start && node == malloc_node_end) {
		malloc_node_start = 0;
		malloc_node_end = 0;
		return;
	}
	node->prev->next = node->next;
	node->next->prev = node->prev;
	if (node == malloc_node_end) {
		malloc_node_end = node->prev;
	}
	if (node == malloc_node_start) {
		malloc_node_start = node->next;
	}
	node->prev = 0;
	node->next = 0;
}

void* safe_malloc_impl(size_t size, char* filename, int line_num) {
	/* Make a node and attaches to end of list. */
	malloc_node* new_node = malloc(size + sizeof(malloc_node));
	if (!new_node) {
		fprintf(stderr, "SafeMalloc: Couldn't allocate memory! %s at line %d.\n",
			filename, line_num);
		safe_exit(2);
		return NULL;
	}
	new_node->filename = filename;
	new_node->line_num = line_num;
	new_node->size = size;
	/* Cast to void to avoid implicit pointer arithmetic */
	new_node->ptr = (void *)new_node + sizeof(malloc_node);
	attach_to_list(new_node);
	return new_node->ptr;
}

void* safe_calloc_impl(size_t num, size_t size, char* filename, int line_num) {
	size *= num;
	malloc_node* new_node = calloc(1, size + sizeof(malloc_node));
	if (!new_node) {
		fprintf(stderr, "SafeCalloc: Couldn't allocate memory! %s at line %d.\n",
			filename, line_num);
		safe_exit(2);
		return NULL;
	}
	new_node->filename = filename;
	new_node->line_num = line_num;
	new_node->size = size;
	new_node->ptr = (void *)new_node + sizeof(malloc_node);
	attach_to_list(new_node);
	return new_node->ptr;
}

void* safe_realloc_impl(void* ptr, size_t size, char* filename, int line_num) {
	malloc_node* core_ptr = ptr - sizeof(*core_ptr);

	void* new_ptr = realloc(core_ptr, size + sizeof(*core_ptr));
	if (!new_ptr) {
		fprintf(stderr, "SafeRealloc: Couldn't reallocate memory! %s at"
			" line %d.\n", filename, line_num);
	}
	// Relink Node
	malloc_node* moved_node = new_ptr;
	moved_node->prev->next = new_ptr;
	moved_node->next->prev = new_ptr;

	if (malloc_node_start == core_ptr) {
		malloc_node_start = new_ptr;
	}

	if (malloc_node_end == core_ptr) {
		malloc_node_end = new_ptr;
	}

	moved_node->ptr = new_ptr + sizeof(malloc_node);
	moved_node->size = size;
	return moved_node->ptr;
}

void safe_free_impl(void* ptr, char* filename, int line_num) {
	UNUSED(filename);
	UNUSED(line_num);
	malloc_node* node_ptr = ptr - sizeof(malloc_node);
	remove_from_list(node_ptr);
	free(node_ptr);
	return;
}

void check_leak() {
	malloc_node* curr = malloc_node_start;
	if (!curr) return;
	do {
		fprintf(stderr, "Memory Leak: %zd bytes (%p), allocated from %s at line %d.\n",
			curr->size, curr->ptr, curr->filename, curr->line_num);
		// Attempt to print the contents of the buffer
		fprintf(stderr, "%.*s\n", (int)curr->size, (char*)curr->ptr);
		curr = curr->next;
	} while (curr != malloc_node_start);
}

void free_alloc() {
	// Should have an empty list!
	malloc_node* curr = malloc_node_start;
	if (!curr) return;
	do {
		free((void*)curr);
		curr = curr->next;
	} while (curr != malloc_node_start);
	return;
}

void safe_exit(int code) {
	free_alloc();
	exit(code);
}

void* safe_release_malloc(size_t size) {
	void* ptr = malloc(size);
	if (!ptr) {
		fprintf(stderr, "Could not allocate memory! Out of memory?");
		safe_exit(2);
		return NULL;
	}
	return ptr;
}

void* safe_release_calloc(size_t num, size_t size) {
	void* ptr = calloc(num, size);
	if (!ptr) {
		fprintf(stderr, "Could not zero allocate memory! Out of memory?");
		safe_exit(2);
		return NULL;
	}
	return ptr;
}

void* safe_release_realloc(void* ptr, size_t size) {
	void* new_ptr = realloc(ptr, size);
	if (!new_ptr) {
		fprintf(stderr, "Could not realloc memory! Out of memory?");
		safe_exit(2);
		return NULL;
	}
	return new_ptr;
}
