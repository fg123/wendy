#include "token.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

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
		// First we see how many bytes
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
			
		printf("%s\n", buffer);
		free(buffer);
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
