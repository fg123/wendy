#ifndef DATA_H
#define DATA_H

#include "global.h"
#include "token.h"

#include <stdio.h>
#include <time.h>

// data.h - Felix Guo
// This module manages the data model for WendyScript

#define FOREACH_DATA(OP) \
	OP(D_EMPTY) \
	OP(D_IDENTIFIER) \
	OP(D_STRING) \
	OP(D_NUMBER) \
	OP(D_INSTRUCTION_ADDRESS) \
	OP(D_INTERNAL_POINTER) \
	OP(D_FUNCTION) \
	OP(D_CLOSURE) \
	OP(D_LIST) \
	OP(D_LIST_HEADER) \
	OP(D_RANGE) \
	OP(D_OBJ_TYPE) \
	OP(D_STRUCT) \
	OP(D_STRUCT_NAME) \
	OP(D_STRUCT_HEADER) \
	OP(D_STRUCT_SHARED) \
	OP(D_STRUCT_PARAM) \
	OP(D_STRUCT_INSTANCE) \
	OP(D_STRUCT_INSTANCE_HEADER) \
	OP(D_STRUCT_METADATA) \
	OP(D_STRUCT_FUNCTION) \
	OP(D_NONERET) \
	OP(D_NONE) \
	OP(D_TRUE) \
	OP(D_FALSE) \
	OP(D_MEMBER_IDENTIFIER) \
	OP(D_NAMED_ARGUMENT_NAME) \
	OP(D_END_OF_ARGUMENTS) \
	OP(D_SPREAD) \
	OP(D_LIST_RANGE_LVALUE) \
	OP(D_ANY) /* No way for client to construct this, can only have a type <any> */ \
	OP(D_TABLE) \
	OP(D_TABLE_INTERNAL_POINTER) \
	OP(D_TABLE_KEY)

enum data_type {
	FOREACH_DATA(ENUM)
};

extern const char* data_string[];

struct data;
struct memory;

union data_value {
	double number;
	char* string;
	struct data* reference;
};

struct data {
	enum data_type type;
	union data_value value;
};

struct list_header {
	size_t size;
	size_t capacity;
};

struct data make_data(enum data_type type, union data_value value);
struct data copy_data(struct data d);
void destroy_data(struct data* d);
void destroy_data_runtime(struct memory* memory, struct data* d);
void destroy_data_runtime_no_ref(struct memory* memory, struct data* d);

// This allows us to trace memory leaks to where it's actually allocated
#define data_value_str(str) data_value_str_impl(safe_strdup(str))
union data_value data_value_str_impl(char *duplicated);

union data_value data_value_num(double num);
union data_value data_value_ptr(struct data* ptr);
union data_value data_value_size(int size);
bool is_numeric(struct data t);
bool is_reference(struct data t);
bool is_vm_internal_type(struct data t);

#define time_data() make_data(D_NUMBER, data_value_num(time(NULL)))
#define noneret_data() make_data(D_NONERET, data_value_str("<noneret>"))
#define none_data() make_data(D_NONE, data_value_str("<none>"))
#define false_data() make_data(D_FALSE, data_value_str("<false>"))
#define any_data() make_data(D_ANY, data_value_str(""))
#define true_data() make_data(D_TRUE, data_value_str("<true>"))

struct data range_data(int start, int end);
int range_start(struct data r);
int range_end(struct data r);
struct data list_header_data(size_t size, size_t capacity);
void print_data(const struct data *t);
bool data_equal(struct data *a, struct data *b);

struct data literal_to_data(struct token literal);
unsigned int print_data_inline(const struct data *t, FILE *buf);

#endif
