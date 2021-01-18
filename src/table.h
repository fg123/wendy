#ifndef TABLE_H
#define TABLE_H

#include <stdlib.h>

#include "data.h"

// HashTable implementation, specifically string -> data
struct entry {
    char* key;
    struct data value;
    struct entry* next;
};

struct table {
    struct entry** buckets;
    size_t bucket_count;
};

size_t get_string_hash(const char* str);

struct table* table_create(void);
void table_destroy(struct memory* memory, struct table* table);
struct table* table_copy(struct table*);

struct data* table_insert(struct table* table, const char* key);
struct data* table_find(struct table* table, const char* key);
bool table_exist(struct table* table, const char* key);

#endif