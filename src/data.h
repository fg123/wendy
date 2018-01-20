#ifndef DATA_H
#define DATA_H

#include "global.h"
#include <stdio.h>

// data.h - Felix Guo
// This module manages the data model for WendyScript

#define FOREACH_DATA(OP) \
    OP(D_EMPTY) \
    OP(D_IDENTIFIER) \
    OP(D_STRING) \
    OP(D_NUMBER) \
    OP(D_ADDRESS) \
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
    OP(D_STRUCT_METADATA) \
    OP(D_STRUCT_SHARED) \
    OP(D_STRUCT_PARAM) \
    OP(D_STRUCT_INSTANCE) \
    OP(D_STRUCT_INSTANCE_HEAD) \
    OP(D_STRUCT_FUNCTION) \
    OP(D_NONERET) \
    OP(D_NONE) \
    OP(D_TRUE) \
    OP(D_FALSE) \
    OP(D_MEMBER_IDENTIFIER) \
    OP(D_ANY)

typedef enum {
    FOREACH_DATA(ENUM)
} data_type;

static const char *data_string[] = {
    FOREACH_DATA(STRING)
};

typedef union {
    double number;
    char* string;
} data_value;

typedef struct {
    data_type type;
    data_value value;
} data;

data make_data(data_type type, data_value value);
data copy_data(data d);
void destroy_data(data* d);

data_value data_value_str(char *str);
data_value data_value_num(double num);
data_value data_value_size(int size);
bool is_numeric(data t);
bool is_boolean(data t);

data time_data();
data noneret_data();
data none_data();
data false_data();
data true_data();
data range_data(int start, int end);
int range_start(data r);
int range_end(data r);
data list_header_data(int size);
void print_data(const data *t);
bool data_equal(data *a, data *b);
unsigned int print_data_inline(const data *t, FILE *buf);

#endif
