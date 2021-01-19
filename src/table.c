#include "table.h"
#include "global.h"
#include "memory.h"

size_t get_string_hash(const char* str) {
    size_t hash = 0;
    for (size_t i = 0; str[i]; i++) {
        hash = 31 * hash + str[i];
    }
    return hash;
}

struct table* table_create(void) {
    struct table* table = safe_malloc(sizeof(struct table));
    table->bucket_count = 16;
    table->buckets = safe_malloc(sizeof(struct entry*) * (table->bucket_count));
    for (size_t i = 0; i < table->bucket_count; i++) {
        table->buckets[i] = 0;
    }
    table->size = 0;
    return table;
}

struct entry* entry_list_copy(struct entry* entry) {
    if (!entry) return NULL;
    struct entry* new_entry = safe_calloc(1, sizeof(struct entry));
    new_entry->key = safe_strdup(entry->key);
    new_entry->value = copy_data(entry->value);
    new_entry->next = entry_list_copy(entry->next);
    return new_entry;
}

struct table* table_copy(struct table* table) {
    struct table* new_table = table_create();
    for (size_t i = 0; i < new_table->bucket_count; i++) {
        new_table->buckets[i] = entry_list_copy(table->buckets[i]);
    }
    return new_table;
}

void buckets_destroy(struct memory* memory, struct entry* entry,
        void (*destroy_routine)(struct memory*, struct data*)) {
    // Destroy linked chain
    if (!entry) return;
    safe_free(entry->key);
    destroy_routine(memory, &entry->value);
    struct entry* next = entry->next;
    safe_free(entry);
    buckets_destroy(memory, next, destroy_routine);
}

void table_destroy(struct memory* memory, struct table* table) {
    // Destroy buckets 
    for (size_t i = 0; i < table->bucket_count; i++) {
        buckets_destroy(memory, table->buckets[i], destroy_data_runtime);
        table->buckets[i] = NULL;
    }
    safe_free(table->buckets);
    table->bucket_count = 0;
    safe_free(table);
}

void table_write_keys_wendy_array(struct table* table, struct data* data) {
    // Start at 1 because data[0] is the list header
    size_t j = 1;
    for (size_t i = 0; i < table->bucket_count; i++) {
        struct entry* buckets = table->buckets[i];
        while (buckets) {
            data[j++] = make_data(D_STRING, data_value_str(buckets->key));
            buckets = buckets->next;
        }
    }
}

void table_destroy_no_ref(struct memory* memory, struct table* table) {
    for (size_t i = 0; i < table->bucket_count; i++) {
        buckets_destroy(memory, table->buckets[i], destroy_data_runtime_no_ref);
        table->buckets[i] = NULL;
    }
    safe_free(table->buckets);
    table->bucket_count = 0;
    safe_free(table);
}

struct data* table_insert(struct table* table, const char* key) {
    // TODO: assume it doesn't already exist.
    size_t bucket = get_string_hash(key) % table->bucket_count;
    
    struct entry* new_entry = safe_calloc(1, sizeof(struct entry));
    new_entry->key = safe_strdup(key);
    new_entry->next = table->buckets[bucket];
    new_entry->value.type = D_EMPTY;
    table->buckets[bucket] = new_entry;
    table->size += 1;
    return &new_entry->value;
}

size_t table_size(struct table* table) {
    return table->size;
}

struct data* table_find(struct table* table, const char* key) {
    size_t bucket = get_string_hash(key) % table->bucket_count;
    struct entry* entry = table->buckets[bucket];
    while (entry) {
        if (streq(entry->key, key)) {
            return &entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

bool table_exist(struct table* table, const char* key) {
    return table_find(table, key) != NULL;
}
