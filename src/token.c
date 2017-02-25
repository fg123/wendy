#include "token.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

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
	strcpy(d.string, s);
	return d;
}

void print_token(const token* t) {
	if (t->t_type == NUMBER) {
		printf("%g\n", t->t_data.number);
	}
	else {
		printf("%s\n", t->t_data.string);
	}
	last_printed_newline = true;
}

void print_token_inline(const token* t) {
	if (t->t_type == NUMBER) {
		printf("%g", t->t_data.number);
		fflush(stdout);
	}
	else {
		printf("%s", t->t_data.string);
		fflush(stdout);
	}
	last_printed_newline = false;
}
