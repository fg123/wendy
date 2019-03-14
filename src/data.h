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
	OP(D_MEMBER) \
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
	OP(D_NAMED_ARGUMENT_NAME) /* For named arguments */ \
	OP(D_END_OF_ARGUMENTS) \
	OP(D_ANY) // No way for client to construct this, can only have a type <any>

typedef enum {
	FOREACH_DATA(ENUM)
} data_type;

extern const char* data_string[];

typedef struct data data;
typedef union {
	double number;
	char* string;
	data* reference;
} data_value;

struct data {
	data_type type;
	data_value value;
};

data make_data(data_type type, data_value value);
data copy_data(data d);
void destroy_data(data* d);

// This allows us to trace memory leaks to where it's actually allocated
#define data_value_str(str) data_value_str_impl(safe_strdup(str))
data_value data_value_str_impl(char *duplicated);

data_value data_value_num(double num);
data_value data_value_ptr(data* ptr);
data_value data_value_size(int size);
bool is_numeric(data t);
bool is_reference(data t);

#define time_data() make_data(D_NUMBER, data_value_num(time(NULL)))
#define noneret_data() make_data(D_NONERET, data_value_str("<noneret>"))
#define none_data() make_data(D_NONE, data_value_str("<none>"))
#define false_data() make_data(D_FALSE, data_value_str("<false>"))
#define any_data() make_data(D_ANY, data_value_str("ANYYYYTHING"))
#define true_data() make_data(D_TRUE, data_value_str("<true>"))

data range_data(int start, int end);
int range_start(data r);
int range_end(data r);
data list_header_data(int size);
void print_data(const data *t);
bool data_equal(data *a, data *b);

data literal_to_data(token literal);
unsigned int print_data_inline(const data *t, FILE *buf);

#endif
