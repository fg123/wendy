#include "token.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "global.h"
#include "memory.h"

bool last_printed_newline = false;
static int line;
static int col;

void set_make_token_param(int l, int c) {
	line = l;
	col = c;
}

token none_token() {
	token t = make_token(NONE, make_data_str("<none>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

token true_token() {
	token t = make_token(TRUE, make_data_str("<true>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

token false_token() {
	token t =  make_token(FALSE, make_data_str("<false>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

token time_token() {
	token t = make_token(NUMBER, make_data_num(time(NULL)));
	t.t_line = line;
	t.t_col = col;
	return t;
}

token noneret_token() {
	token t = make_token(NONERET, make_data_str("<noneret>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

token empty_token() {
	token t = make_token(EMPTY, make_data_str(""));
	t.t_line = line;
	t.t_col = col;
	return t;
}

token range_token(int start, int end) {
	token res = make_token(RANGE, make_data_str(""));
	sprintf(res.t_data.string, "%d|%d", start, end);
	res.t_line = line;
	res.t_col = col;
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
	token token_ = { t, 0, 0, d };
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
	}
	else if (t->t_type == STRUCT) {
		fprintf(buf, "<struct>");
	}
	else if (t->t_type == FUNCTION) {
		fprintf(buf, "<function>");
	}
	else if (t->t_type == STRUCT_INSTANCE) {
		token instance_loc = memory[(int)(t->t_data.number)];
		fprintf(buf, "<struct:%s>", 
				memory[(int)instance_loc.t_data.number + 1].t_data.string);
	}
	else if (t->t_type == RANGE) {
		fprintf(buf, "<range from %d to %d>", range_start(*t), range_end(*t));
	}
	else if (t->t_type == LIST_HEADER) {
		fprintf(buf, "<lhd size %d>", (int)(t->t_data.number));	
	}
	else if (t->t_type == STRUCT_METADATA) {
		fprintf(buf, "<meta size %d>", (int)(t->t_data.number));	
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
	}
	else if (t->t_type == NUMBER) {
		size_t len = snprintf(0, 0, "%f", t->t_data.number);
//		printf("length %d\n", len);
		char* buffer = safe_malloc(len + 1);
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
		safe_free(buffer);
	}
	else if (t->t_type == ADDRESS) {
		fprintf(buf, "0x%X", (int)t->t_data.number);
	}
	/*else if (t->t_type == ARRAY_HEADER) {
		fprintf(buf, "<array size %d>", t->t_data.number);
		fflush(buf);
	}*/
	else {
		fprintf(buf, "%s", t->t_data.string);
	}
	last_printed_newline = false;
	fflush(buf);
}

int precedence(token op) {
	switch (op.t_type) {
		case PLUS:
		case MINUS:
			return 140;
		case STAR:
		case SLASH:
		case INTSLASH:
		case PERCENT:
			return 150;
		case AND:
			return 120;
		case OR:
			return 110;
		case RANGE_OP:
		/*case TYPEOF:*/
			return 132;
		case NOT_EQUAL:
		case EQUAL_EQUAL:
		case TILDE:
			return 130;
		case GREATER:
		case GREATER_EQUAL:
		case LESS:
		case LESS_EQUAL:
			return 130;
		case NOT:
		case U_MINUS:
	    case U_TILDE:
		case U_STAR:
			return 160;
		case DOT:
		case LEFT_BRACK:
			// Array bracket
			return 170;
		case AMPERSAND:
			// Actually precedes the left bracket!?
			return 180;
		default: 
			return 0;
	}
}

bool is_numeric(token t) {
	return t.t_type == NUMBER || t.t_type == ADDRESS || t.t_type == LIST ||
		t.t_type == LIST_HEADER || t.t_type == STRUCT || 
		t.t_type == STRUCT_METADATA || t.t_type == STRUCT_INSTANCE || 
		t.t_type == STRUCT_INSTANCE_HEAD;
}

