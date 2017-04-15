#include "token.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "macros.h"
#include "memory.h"

bool last_printed_newline = false;

token none_token() {
	return make_token(NONE, make_data_str("<none>"));
}

token true_token() {
	return make_token(TRUE, make_data_str("<true>"));
}

token false_token() {
	return make_token(FALSE, make_data_str("<false>"));
}

token time_token() {
	return make_token(NUMBER, make_data_num(time(NULL)));
}

token noneret_token() {
	return make_token(NONERET, make_data_str("<noneret>"));
}

token range_token(int start, int end) {
	token res = make_token(RANGE, make_data_str(""));
	sprintf(res.t_data.string, "%d|%d", start, end);
	return res;
}

int range_start(token r) {
	int start = 0;
	int end = 0;
	sscanf(r.t_data.string, "%d|%d", &start, &end);
	return start;
}

int range_end(token r) {
	int start = 0;
	int end = 0;
	sscanf(r.t_data.string, "%d|%d", &start, &end);
	return end;
}

token list_header_token(int size) {
	//char* str = calloc(MAX_STRING_LEN, sizeof(char));
	//sprintf(str, "<list size %d>", size);

/*	int index = MAX_STRING_LEN - 1;
	while (size > 0) {
		str[index--] = size % 128;
		size /= 128;
	}*/
	token res = make_token(LIST_HEADER, make_data_num(size));
	//free(str);
	return res;
}

/*int get_list_size(token t) {
	if (t.t_type == LIST) {
		int size = 0;
		int index = MAX_STRING_LEN - 1;
		while (t.t_data.string[index] != 0) {
			size += t.t_data.string[index--];
		}
		return size;
	}
	return 0;
}*/

token make_token(token_type t, data d) {
	token token_ = { t, 0, d };
	return token_;
}

data make_data_num(double i) {
	data d;
	d.number = i;
	return d;
}

data make_data_str(char* s) {
	data d;
	memcpy(d.string, s, MAX_STRING_LEN);
	return d;
}

bool token_equal(token* a, token* b) {
	if (a->t_type != b->t_type) {
		return false;
	}
	else {
		if (a->t_type == NUMBER || a->t_type == ADDRESS) {
			return a->t_data.number == b->t_data.number;
		}
		else {
			return strcmp(a->t_data.string, b->t_data.string) == 0;
		}
	}
}

void print_token(const token* t) {
	print_token_inline(t, stdout);
	printf("\n");
	last_printed_newline = true;
	fflush(stdout);
}

void print_token_inline(const token* t, FILE* buf) {
	if (t->t_type == OBJ_TYPE) {
		fprintf(buf, "<%s>", t->t_data.string);
		fflush(buf);
	}
	else if (t->t_type == STRUCT) {
		fprintf(buf, "<struct>");
		fflush(buf);
	}
	else if (t->t_type == FUNCTION) {
		fprintf(buf, "<function>");
		fflush(buf);
	}
	else if (t->t_type == STRUCT_INSTANCE) {
		token instance_loc = memory[(int)(t->t_data.number)];
		fprintf(buf, "<struct:%s>", 
				memory[(int)instance_loc.t_data.number + 1].t_data.string);
	}
	else if (t->t_type == RANGE) {
		fprintf(buf, "<range from %d to %d>", range_start(*t), range_end(*t));
		fflush(buf);
	}
	else if (t->t_type == LIST) {
		// Traverse to list header
		address start = t->t_data.number;
		token l_header = memory[start];
		fprintf(buf, "[");
		for (int i = 0; i < l_header.t_data.number; i++) {
			if (i != 0) fprintf(buf, ", ");
			print_token_inline(&memory[start + i + 1], buf);
		}
		fprintf(buf, "]");
		fflush(buf);
	}
	else if (t->t_type == NUMBER || t->t_type == ADDRESS) {
		size_t len = snprintf(0, 0, "%f", t->t_data.number);
//		printf("length %d\n", len);
		char* buffer = malloc(len + 1);
//		memset(buffer, 0, len + 1);
		snprintf(buffer, len + 1, "%f", t->t_data.number);
		// Start at end, if it's 0 we clip it, otherwise we stop
		len--;
		while (buffer[len] == '0') {
			buffer[len--] = 0;
		}
		if (buffer[len] == '.') {
			// clip that too
			buffer[len--] = 0;
		}
			
		fprintf(buf, "%s", buffer);
		fflush(buf);
	}
	/*else if (t->t_type == ARRAY_HEADER) {
		fprintf(buf, "<array size %d>", t->t_data.number);
		fflush(buf);
	}*/
	else {
		fprintf(buf, "%s", t->t_data.string);
		fflush(buf);
	}
	last_printed_newline = false;
}
