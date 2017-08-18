#include "token.h"
#include "global.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

bool last_printed_newline = false;
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

token time_token() {
    token t = make_token(T_NUMBER, make_data_num(time(NULL)));
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

token empty_token() {
    token t = make_token(T_EMPTY, make_data_str(""));
    t.t_line = line;
    t.t_col = col;
    return t;
}

token range_token(int start, int end) {
    token res = make_token(T_RANGE, make_data_str(""));
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
    token res = make_token(T_LIST_HEADER, make_data_num(size));
    return res;
}

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
    memcpy(d.string, s, MAX_STRING_LEN * sizeof(char));
    return d;
}

bool token_equal(token* a, token* b) {
    if (a->t_type != b->t_type) {
        return false;
    }
    else {
        if (a->t_type == T_NUMBER || a->t_type == T_ADDRESS) {
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
    if (t->t_type == T_OBJ_TYPE) {
        fprintf(buf, "<%s>", t->t_data.string);
    }
    else if (t->t_type == T_STRUCT) {
        fprintf(buf, "<struct>");
    }
    else if (t->t_type == T_FUNCTION) {
        fprintf(buf, "<function>");
    }
    else if (t->t_type == T_STRUCT_FUNCTION) {
        fprintf(buf, "<struct function>");
    }
    else if (t->t_type == T_STRUCT_INSTANCE) {
        token instance_loc = memory[(int)(t->t_data.number)];
        fprintf(buf, "<struct:%s>", 
                memory[(int)instance_loc.t_data.number + 1].t_data.string);
    }
    else if (t->t_type == T_RANGE) {
        fprintf(buf, "<range from %d to %d>", range_start(*t), range_end(*t));
    }
    else if (t->t_type == T_LIST_HEADER) {
        fprintf(buf, "<lhd size %d>", (int)(t->t_data.number)); 
    }
    else if (t->t_type == T_STRUCT_METADATA) {
        fprintf(buf, "<meta size %d>", (int)(t->t_data.number));    
    }
    else if (t->t_type == T_LIST) {
        address start = t->t_data.number;
        token l_header = memory[start];
        fprintf(buf, "[");
        for (int i = 0; i < l_header.t_data.number; i++) {
            if (i != 0) fprintf(buf, ", ");
            print_token_inline(&memory[start + i + 1], buf);
        }
        fprintf(buf, "]");
    }
    else if (t->t_type == T_NUMBER) {
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
            
        fprintf(buf, "%s", buffer);
        safe_free(buffer);
    }
    else if (t->t_type == T_ADDRESS) {
        fprintf(buf, "0x%X", (int)t->t_data.number);
    }
    else {
        fprintf(buf, "%s", t->t_data.string);
    }
    last_printed_newline = false;
    fflush(buf);
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
        case T_U_MINUS:
        case T_U_TILDE:
        case T_U_STAR:
            return 160;
        case T_DOT:
        case T_LEFT_BRACK:
            return 170;
        case T_AMPERSAND:
            return 180;
        default: 
            return 0;
    }
}

bool is_numeric(token t) {
    return t.t_type == T_NUMBER || t.t_type == T_ADDRESS || t.t_type == T_LIST ||
        t.t_type == T_LIST_HEADER || t.t_type == T_STRUCT || t.t_type == T_FUNCTION ||
        t.t_type == T_STRUCT_METADATA || t.t_type == T_STRUCT_INSTANCE || 
        t.t_type == T_STRUCT_INSTANCE_HEAD;
}

bool is_boolean(token t) {
    return t.t_type == T_TRUE || t.t_type == T_FALSE;
}