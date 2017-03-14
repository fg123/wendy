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

token noneret_token() {
	return make_token(NONERET, make_data_str("<noneret>"));
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

bool token_equal(token* a, token* b) {
	if (a->t_type != b->t_type) {
		return false;
	}
	else {
		if (a->t_type == NUMBER) {
			return a->t_data.number == b->t_data.number;
		}
		else {
			return strcmp(a->t_data.string, b->t_data.string) == 0;
		}
	}
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

void print_token_inline(const token* t, FILE* buf) {
	if (t->t_type == NUMBER) {
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
	else {
		fprintf(buf, "%s", t->t_data.string);
		fflush(buf);
	}
	last_printed_newline = false;
}
