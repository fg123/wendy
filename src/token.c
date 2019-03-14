#include "token.h"
#include "global.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

const char* token_string[] = {
	FOREACH_TOKEN(STRING)
};

static int line;
static int col;

void set_make_token_param(int l, int c) {
	line = l;
	col = c;
}

struct token none_token() {
	struct token t = make_token(T_NONE, make_data_str("<none>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

struct token true_token() {
	struct token t = make_token(T_TRUE, make_data_str("<true>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

struct token false_token() {
	struct token t = make_token(T_FALSE, make_data_str("<false>"));
	t.t_line = line;
	t.t_col = col;
	return t;
}

struct token empty_token() {
	struct token t = make_token(T_EMPTY, make_data_str(""));
	t.t_line = line;
	t.t_col = col;
	return t;
}

struct token make_token(enum token_type t, union token_data d) {
	struct token token_ = { t, 0, 0, d };
	return token_;
}

union token_data make_data_num(double i) {
	union token_data d;
	d.number = i;
	return d;
}

union token_data make_data_str(char* s) {
	union token_data d;
	d.string = safe_strdup(s);
	return d;
}

void print_token(const struct token* t) {
	print_token_inline(t, stdout);
	printf("\n");
	last_printed_newline = true;
	fflush(stdout);
}

unsigned int print_token_inline(const struct token* t, FILE* buf) {
	unsigned int p = 0;
	if (t->t_type == T_NUMBER) {
		size_t len = snprintf(0, 0, "%f", t->t_data.number);
		char* buffer = safe_malloc(len + 1);
		snprintf(buffer, len + 1, "%f", t->t_data.number);
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
	else {
		p += fprintf(buf, "%s", t->t_data.string);
	}
	last_printed_newline = false;
	fflush(buf);
	return p;
}

int precedence(struct token op) {
	switch (op.t_type) {
		case T_PLUS:
		case T_MINUS:
			return 140;
		case T_STAR:
		case T_SLASH:
		case T_INTSLASH:
		case T_PERCENT:
			return 150;
		case T_AND:
			return 120;
		case T_OR:
			return 110;
		case T_RANGE_OP:
			return 132;
		case T_NOT_EQUAL:
		case T_EQUAL_EQUAL:
		case T_TILDE:
			return 130;
		case T_GREATER:
		case T_GREATER_EQUAL:
		case T_LESS:
		case T_LESS_EQUAL:
			return 130;
		case T_NOT:
			return 160;
		case T_DOT:
		case T_LEFT_BRACK:
			return 170;
			return 180;
		default:
			return 0;
	}
}

inline void destroy_token(struct token l) {
	if (l.t_type != T_NUMBER) {
        safe_free(l.t_data.string);
    }
}

void free_token_list(struct token* l, size_t s) {
	for (size_t i = 0; i < s; i++) {
        destroy_token(l[i]);
	}
	safe_free(l);
}
