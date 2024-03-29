#include "data.h"
#include "global.h"
#include "memory.h"
#include "error.h"
#include "table.h"

#include <string.h>
#include <stdbool.h>

// Implementation for Data Model

const char* data_string[] = {
	FOREACH_DATA(STRING)
	0 // Sentinal value used when traversing through this array; acts as a NULL
};

struct data make_data(enum data_type type, union data_value value) {
	struct data _data = { type, value };
	return _data;
}

struct data copy_data(struct data d) {
	if (d.type == D_LIST_HEADER) {
		struct list_header* header = (struct list_header*) d.value.reference;
		return list_header_data(header->size, header->capacity);
	}
	else if (d.type == D_TABLE_INTERNAL_POINTER) {
		error_general("Can't copy a D_TABLE_INTERNAL_POINTER!\n");
		return none_data();
		// return make_data(d.type, data_value_ptr((struct data*) table_copy(
		// 	(struct table*) d.value.reference
		// )));
	}
	else if (is_numeric(d)) {
		return make_data(d.type, data_value_num(d.value.number));
	}
	else if (is_reference(d)) {
		return make_data(d.type, data_value_ptr(
			refcnt_copy(d.value.reference)));
	}
	else {
		return make_data(d.type, data_value_str(d.value.string));
	}
}

bool data_equal(struct data* a, struct data* b) {
	if (a->type != b->type) {
		return false;
	}
	else {
		if (is_numeric(*a)) {
			return a->value.number == b->value.number;
		}
		else {
			return streq(a->value.string, b->value.string);
		}
	}
}

// This is for RUNTIME DATA DESTRUCTION
void destroy_data_runtime(struct memory* memory, struct data* d) {
	if (d->type == D_LIST_HEADER) {
		safe_free(d->value.reference);
	}
	else if (d->type == D_TABLE_INTERNAL_POINTER) {
		table_destroy(memory, (struct table*)d->value.reference);
	}
	else if (is_reference(*d)) {
		refcnt_free(memory, d->value.reference);
	}
	else if (!is_numeric(*d)) {
		safe_free(d->value.string);
	}

	d->type = D_EMPTY;
}

void destroy_data_runtime_no_ref(struct memory* memory, struct data* d) {
	// Don't follow references, since we're in the final freeing state
	if (d->type == D_LIST_HEADER) {
		safe_free(d->value.reference);
	}
	else if (d->type == D_TABLE_INTERNAL_POINTER) {
		table_destroy_no_ref(memory, (struct table*)d->value.reference);
	}
	else if (is_reference(*d)) {
		return;
	}
	else if (!is_numeric(*d)) {
		safe_free(d->value.string);
	}

	d->type = D_EMPTY;
}

void destroy_data(struct data* d) {
	if (d->type == D_LIST_HEADER) {
		safe_free(d->value.reference);
	}
	else if (d->type == D_TABLE_INTERNAL_POINTER) {
		error_general("Tried to destroy a table without a memory instance!");
	}
	else if (is_reference(*d)) {
		error_general("Tried to destroy a reference data type without a memory instance!");
	}
	else if (!is_numeric(*d)) {
		safe_free(d->value.string);
	}
	d->type = D_EMPTY;
}

union data_value data_value_str_impl(char *duplicated) {
	union data_value r;
	r.string = duplicated;
	return r;
}

union data_value data_value_ptr(struct data* ptr) {
	union data_value r;
	r.reference = ptr;
	return r;
}

union data_value data_value_size(int size) {
	union data_value r;
	r.string = safe_calloc((size + 1), sizeof(char));
	return r;
}

union data_value data_value_num(double num) {
	union data_value r;
	r.number = num;
	return r;
}

bool is_reference(struct data t) {
	return t.type == D_STRUCT ||
		t.type == D_LIST ||
		t.type == D_FUNCTION ||
		t.type == D_STRUCT_FUNCTION ||
		t.type == D_STRUCT_METADATA ||
		t.type == D_CLOSURE ||
		t.type == D_SPREAD ||
		t.type == D_TABLE ||
		t.type == D_STRUCT_INSTANCE ||
		t.type == D_STRUCT_FUNCTION ||
		t.type == D_LIST_RANGE_LVALUE;
}

bool is_numeric(struct data t) {
	return t.type == D_NUMBER ||
		t.type == D_INSTRUCTION_ADDRESS ||
		t.type == D_STRUCT_HEADER||
		t.type == D_STRUCT_INSTANCE_HEADER ||
		t.type == D_EMPTY ||
		t.type == D_RANGE ||
		t.type == D_END_OF_ARGUMENTS ||

		// This uses "reference" to point to a struct list_header
		t.type == D_LIST_HEADER ||

		// This uses "reference" to point to a table
		t.type == D_TABLE_INTERNAL_POINTER ||

		// This is actually a "reference" type, but because the
		// corresponding pointer is not ref-counted, we label it
		// as numeric.
		t.type == D_INTERNAL_POINTER;
}

bool is_vm_internal_type(struct data t) {
	return t.type != D_STRING &&
		t.type != D_NUMBER &&
		t.type != D_FUNCTION &&
		t.type != D_LIST &&
		t.type != D_RANGE &&
		t.type != D_STRUCT &&
		t.type != D_STRUCT_INSTANCE &&
		t.type != D_OBJ_TYPE &&
		t.type != D_NONE &&
		t.type != D_TRUE &&
		t.type != D_FALSE;
}

struct data range_data(int start, int end) {
	// Box 2 integers into the space of the double
	struct data res = make_data(D_RANGE, data_value_num(0));
	int* start_ptr = (int*) &res.value.number;
	int* end_ptr = (int*)((void*)&res.value.number + sizeof(int));
	*start_ptr = start;
	*end_ptr = end;
	return res;
}

int range_start(struct data r) {
	int* start_ptr = (int*) &r.value.number;
	return *start_ptr;
}

int range_end(struct data r) {
	int* end_ptr = (int*)((void*)&r.value.number + sizeof(int));
	return *end_ptr;
}

struct data list_header_data(size_t size, size_t capacity) {
	struct list_header* header = safe_malloc(sizeof(struct list_header));
	header->size = size;
	header->capacity = capacity;
	struct data res = make_data(D_LIST_HEADER, data_value_ptr((struct data*) header));
	return res;
}

void print_data(const struct data* t) {
	print_data_inline(t, stdout);
	printf("\n");
	last_printed_newline = true;
	fflush(stdout);
}

unsigned int print_params_if_available(FILE* buf, const struct data* function_data) {
	struct data list_ref = function_data->value.reference[3];
	if (list_ref.type != D_LIST) return 0;
	unsigned int p = 0;
	unsigned int size = wendy_list_size(&list_ref);
	p += fprintf(buf, "(");
	for (unsigned int i = 0; i < size; i++) {
		if (i != 0) {
			p += fprintf(buf, ", ");
		}
		p += fprintf(buf, "%s", list_ref.value.reference[i + 1].value.string);
	}
	p += fprintf(buf, ")");
	return p;
}

unsigned int print_data_inline(const struct data* t, FILE* buf) {
	unsigned int p = 0;
	if (t->type == D_OBJ_TYPE) {
		p += fprintf(buf, "<%s>", t->value.string);
	}
	else if (t->type == D_STRUCT) {
		p += fprintf(buf, "<struct>");
	}
	else if (t->type == D_SPREAD) {
		p += fprintf(buf, "<spread: ");
		p += print_data_inline(t->value.reference, buf);
		p += fprintf(buf, ">");
	}
	else if (t->type == D_FUNCTION) {
		p += fprintf(buf, "<function");
		p += print_params_if_available(buf, t);
		p += fprintf(buf, ">");
	}
	else if (t->type == D_STRUCT_FUNCTION) {
		p += fprintf(buf, "<struct function");
		p += print_params_if_available(buf, t);
		p += fprintf(buf, ">");
	}
	else if (t->type == D_END_OF_ARGUMENTS) {
		p += fprintf(buf, "<eoargs>");
	}
	else if (t->type == D_STRUCT_INSTANCE) {
		p += fprintf(buf, "<struct:%s>", t->value.reference[1].value.reference[1].value.string);
	}
	else if (t->type == D_RANGE) {
		p += fprintf(buf, "<range from %d to %d>", range_start(*t), range_end(*t));
	}
	else if (t->type == D_LIST_HEADER) {
		p += fprintf(buf, "<lhd size %d>", (int)(t->value.number));
	}
	else if (t->type == D_STRUCT_HEADER) {
		p += fprintf(buf, "<meta size %d>", (int)(t->value.number));
	}
	else if (t->type == D_TABLE) {
		p += fprintf(buf, "<table>");
	}
	else if (t->type == D_LIST || t->type == D_CLOSURE) {
		// Special case here because closures are implemented as lists
		size_t size = wendy_list_size(t);
		p += fprintf(buf, "[");
		for (size_t i = 0; i < size; i++) {
			if (i != 0) p += fprintf(buf, ", ");
			p += print_data_inline(&t->value.reference[i + 1], buf);
		}
		p += fprintf(buf, "]");
	}
	else if (t->type == D_NAMED_ARGUMENT_NAME) {
		p += fprintf(buf, "named: %s", t->value.string);
	}
	else if (t->type == D_NUMBER) {
		size_t len = snprintf(0, 0, "%f", t->value.number);
		char* buffer = safe_malloc(len + 1);
		snprintf(buffer, len + 1, "%f", t->value.number);
		len--;
		while (buffer[len] == '0') {
			buffer[len--] = 0;
		}
		if (buffer[len] == '.') {
			buffer[len--] = 0;
		}

		p += fprintf(buf, "%s", buffer);
		safe_free(buffer);
	}
	else if (t->type == D_INSTRUCTION_ADDRESS) {
		p += fprintf(buf, "@0x%X", (int) t->value.number);
	}
	else if (is_numeric(*t)) {
		p += fprintf(buf, "[%s] 0x%X", data_string[t->type],
			(int)t->value.number);
	}
	else {
		p += fprintf(buf, "%s", t->value.string);
	}
	last_printed_newline = false;
	fflush(buf);
	return p;
}

static enum data_type literal_type_to_data_type(enum token_type t) {
	switch(t) {
		case T_NUMBER:
			return D_NUMBER;
		case T_STRING:
			return D_STRING;
		case T_TRUE:
			return D_TRUE;
		case T_FALSE:
			return D_FALSE;
		case T_NONE:
			return D_NONE;
		case T_IDENTIFIER:
			return D_IDENTIFIER;
		case T_OBJ_TYPE:
			return D_OBJ_TYPE;
		case T_MEMBER:
			return D_MEMBER_IDENTIFIER;
		default:;
	}
	return D_EMPTY;
}

struct data literal_to_data(struct token literal) {
	if (literal.t_type == T_NUMBER) {
		return make_data(D_NUMBER, data_value_num(literal.t_data.number));
	}
	return make_data(literal_type_to_data_type(literal.t_type),
		data_value_str(literal.t_data.string));
}
