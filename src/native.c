#include "native.h"
#include "memory.h"
#include "error.h"
#include "data.h"
#include "codegen.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct native_function {
    char* name;
    int argc;
    data (*function)(data*);
} native_function;

static data native_printCallStack(data* args);
static data native_reverseString(data* args);
static data native_stringToInteger(data* args);
static data native_examineMemory(data* args);
static data native_exec(data* args);
static data native_getc(data* args);
static data native_printBytecode(data* args);
static data native_garbageCollect(data* args);
static data native_printFreeMemory(data* args);

static native_function native_functions[] = {
    { "printCallStack", 1, native_printCallStack },
    { "reverseString", 1, native_reverseString },
    { "stringToInteger", 1, native_stringToInteger },
    { "examineMemory", 2, native_examineMemory },
    { "exec", 1, native_exec },
    { "getc", 0, native_getc },
    { "printBytecode", 0, native_printBytecode },
    { "garbageCollect", 0, native_garbageCollect },
    { "printFreeMemory", 0, native_printFreeMemory }
};

static double native_to_numeric(data* t) {
    return t->value.number;
}

static char* native_to_string(data* t) {
    return t->value.string;
}

static data native_garbageCollect(data* args) {
    garbage_collect(0);
    return noneret_data();
}

static data native_printFreeMemory(data* args) {
    print_free_memory();
    return noneret_data();
}

static data native_getc(data* args) {
    char result[2];
    result[0] = getc(stdin);
    result[1] = 0;
    return make_data(T_STRING, data_value_str(result));
}

static data native_exec(data* args) {
    char* command = native_to_string(args);
    //system(command);
    return noneret_data();
}

static data native_printCallStack(data* args) {
    print_call_stack((int)native_to_numeric(args));
    return noneret_data();
}

static data native_printBytecode(data* args) {
    print_current_bytecode();
    return noneret_data();
}

static data native_reverseString(data* args) {
    char* string = native_to_string(args);
    int len = strlen(string);
    data t = make_data(D_STRING, data_value_str(string));
    for (int i = 0; i < len / 2; i++) {
        char tmp = t.value.string[i];
        t.value.string[i] = t.value.string[len - i - 1];
        t.value.string[len - i - 1] = tmp;
    }
    return t;
}

static data native_stringToInteger(data* args) {
    char* s = native_to_string(args);
    int integer = 0;
    bool neg = false;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '-') {
            neg = !neg;
        }
        else if (s[i] >= '0' && s[i] <= '9') {
            integer *= 10;
            integer += s[i] - '0';
        }
    }
    if (neg) integer *= -1;
    return make_data(D_NUMBER, data_value_num(integer));
}

static data native_examineMemory(data* args) {
    double arg1_from = native_to_numeric(args);
    double arg2_to = native_to_numeric(args + 1);
    printf("Memory Contents: \n");
    for (int i = arg1_from; i < arg2_to; i++) {
        data t = memory[i];
        printf("[0x%04X] [%s] ", i, data_string[t.type]);
        if (is_numeric(t)) {
            printf("[%f][%d][0x%X]", t.value.number, (int)t.value.number,
                (int)t.value.number);
        }
        else {
            printf("[%s]", t.value.string);
        }
        printf("\n");
    }
    return noneret_data();
}

void native_call(char* function_name, int expected_args, int line) {
    int functions = sizeof(native_functions) / sizeof(native_functions[0]);
    bool found = false;
    for (int i = 0; i < functions; i++) {
        if(streq(native_functions[i].name, function_name)) {
            int argc = native_functions[i].argc;
            data* arg_list = safe_malloc(sizeof(data) * argc);
            // Popped in reverse order
            for (int j = argc - 1; j >= 0; j--) {
                arg_list[j] = pop_arg(line);
            }
            push_arg(native_functions[i].function(arg_list), line);
            safe_free(arg_list);
            found = true;
            break;
        }
    }
    if (!found) {
        error_runtime(line, VM_INVALID_NATIVE_CALL, function_name);
    }
}
