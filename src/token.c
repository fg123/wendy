#include "token.h"
#include "global.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

static int line;
static int col;

void set_make_token_param(int l, int c) {
    line = l;
    col = c;
}

token none_token() {
    token t = make_token(T_NONE, make_data_str("<none>"));
    t.t_line = line;
    t.t_col = col;
    return t;
}

token noneret_token() {
    token t = make_token(T_NONERET, make_data_str("<noneret>"));
    t.t_line = line;
    t.t_col = col;
    return t;
}

token true_token() {
    token t = make_token(T_TRUE, make_data_str("<true>"));
    t.t_line = line;
    t.t_col = col;
    return t;
}

token false_token() {
    token t =  make_token(T_FALSE, make_data_str("<false>"));
    t.t_line = line;
    t.t_col = col;
    return t;
}

token empty_token() {
    token t = make_token(T_EMPTY, make_data_str(""));
    t.t_line = line;
    t.t_col = col;
    return t;
}

token make_token(token_type t, token_data d) {
    token token_ = { t, 0, 0, d };
    return token_;
}

token_data make_data_num(double i) {
    token_data d;
    d.number = i;
    return d;
}

token_data make_data_str(char* s) {
    token_data d;
    memcpy(d.string, s, MAX_STRING_LEN * sizeof(char));
    return d;
}

void print_token(const token* t) {
    print_token_inline(t, stdout);
    printf("\n");
    last_printed_newline = true;
    fflush(stdout);
}

unsigned int print_token_inline(const token* t, FILE* buf) {
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

int precedence(token op) {
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
