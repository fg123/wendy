#include "data.h"
#include "global.h"
#include "memory.h"
#include "time.h"
#include <string.h>
#include <stdbool.h>
#include <time.h>

// Implementation for Data Model

const char* data_string[] = {
	FOREACH_DATA(STRING)
	0 // Sentinal value used when traversing through this array; acts as a NULL
};

data make_data(data_type type, data_value value) {
	data _data = { type, value };
	return _data;
}

data copy_data(data d) {
	if (is_numeric(d)) {
		return make_data(d.type, data_value_num(d.value.number));
	}
	else {
		return make_data(d.type, data_value_str(d.value.string));
	}
}

bool data_equal(data* a, data* b) {
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

void destroy_data(data* d) {
	if (!is_numeric(*d)) {
		safe_free(d->value.string);
	}
	d->type = D_EMPTY;
}

data_value data_value_str(char *str) {
	data_value r;
	r.string = strdup(str);
	return r;
}

data_value data_value_size(int size) {
	data_value r;
	r.string = safe_calloc((size + 1), sizeof(char));
	return r;
}

data_value data_value_num(double num) {
	data_value r;
	r.number = num;
	return r;
}

bool is_numeric(data t) {
	return t.type == D_NUMBER || t.type == D_ADDRESS || t.type == D_LIST ||
		t.type == D_LIST_HEADER || t.type == D_STRUCT || t.type == D_FUNCTION ||
		t.type == D_STRUCT_METADATA || t.type == D_STRUCT_INSTANCE ||
		t.type == D_STRUCT_INSTANCE_HEAD || t.type == D_STRUCT_FUNCTION ||
		t.type == D_CLOSURE ||
		t.type == D_EMPTY || t.type == D_INTERNAL_POINTER;
}

bool is_boolean(data t) {
	return t.type == D_TRUE || t.type == D_FALSE;
}

data time_data() {
	data t = make_data(D_NUMBER, data_value_num(time(NULL)));
	return t;
}

data false_data() {
	data t = make_data(D_FALSE, data_value_str("<false>"));
	return t;
}

data true_data() {
	data t = make_data(D_TRUE, data_value_str("<true>"));
	return t;
}

data none_data() {
	data t = make_data(D_NONE, data_value_str("<none>"));
	return t;
}

data any_data() {
	data t = make_data(D_ANY, data_value_str("used to refer to one or some of "
		"a thing or number of things, no matter how much or many."));
	return t;
}

data noneret_data() {
	data t = make_data(D_NONERET, data_value_str("<noneret>"));
	return t;
}

data range_data(int start, int end) {
	data res = make_data(D_RANGE, data_value_str(""));
	sprintf(res.value.string, "%d|%d", start, end);
	return res;
}

int range_start(data r) {
	int start = 0;
	int end = 0;
	sscanf(r.value.string, "%d|%d", &start, &end);
	return start;
}

int range_end(data r) {
	int start = 0;
	int end = 0;
	sscanf(r.value.string, "%d|%d", &start, &end);
	return end;
}

data list_header_data(int size) {
	data res = make_data(D_LIST_HEADER, data_value_num(size));
	return res;
}

void print_data(const data* t) {
	print_data_inline(t, stdout);
	printf("\n");
	last_printed_newline = true;
	fflush(stdout);
}

unsigned int print_data_inline(const data* t, FILE* buf) {
	unsigned int p = 0;
	if (t->type == D_OBJ_TYPE) {
		p += fprintf(buf, "<%s>", t->value.string);
	}
	else if (t->type == D_STRUCT) {
		p += fprintf(buf, "<struct>");
	}
	else if (t->type == D_FUNCTION) {
		p += fprintf(buf, "<function>");
	}
	else if (t->type == D_STRUCT_FUNCTION) {
		p += fprintf(buf, "<struct function>");
	}
	else if (t->type == D_STRUCT_INSTANCE) {
		data instance_loc = memory[(int)(t->value.number)];
		p += fprintf(buf, "<struct:%s>",
				memory[(int)instance_loc.value.number + 1].value.string);
	}
	else if (t->type == D_RANGE) {
		p += fprintf(buf, "<range from %d to %d>", range_start(*t), range_end(*t));
	}
	else if (t->type == D_LIST_HEADER) {
		p += fprintf(buf, "<lhd size %d>", (int)(t->value.number));
	}
	else if (t->type == D_STRUCT_METADATA) {
		p += fprintf(buf, "<meta size %d>", (int)(t->value.number));
	}
	else if (t->type == D_LIST) {
		address start = t->value.number;
		data l_header = memory[start];
		p += fprintf(buf, "[");
		for (int i = 0; i < l_header.value.number; i++) {
			if (i != 0) p += fprintf(buf, ", ");
			p += print_data_inline(&memory[start + i + 1], buf);
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
