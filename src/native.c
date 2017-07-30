// Implementation of Native

#include "native.h"
#include "memory.h"
#include "error.h"
#include "token.h"
#include <string.h>

static double native_to_numeric(token* t) {
    return t->t_data.number;
}

static char* native_to_string(token* t) {
    return t->t_data.string;
}

static void native_printCallStack(double arg1) {
    print_call_stack((int)arg1);
}

static void native_examineMemory(double arg1_from, double arg2_to) {
    printf("Memory Contents: \n");
    for (int i = arg1_from; i < arg2_to; i++) {
        token t = memory[i];
        printf("[0x%04X] [%s] ", i, token_string[t.t_type]);
        if (is_numeric(t)) {
            printf("[%f][%d][0x%X]", t.t_data.number, (int)t.t_data.number, 
                (int)t.t_data.number);
        }
        else {
            printf("[%s]", t.t_data.string);
        }
        printf("\n");
    }
}

void native_call(char* function_name, int expected_args, int line) {
    if (strcmp(function_name, "printCallStack") == 0) {
        token arg1 = pop_arg(line);
        native_printCallStack(native_to_numeric(&arg1));
    }
    else if (strcmp(function_name, "printFreeMemory") == 0) {
        print_free_memory();
    }
    else if (strcmp(function_name, "examineMemory") == 0) {
        token arg2 = pop_arg(line);
        token arg1 = pop_arg(line);
        native_examineMemory(native_to_numeric(&arg1), 
            native_to_numeric(&arg2));
    }
    else {
        error_runtime(line, VM_INVALID_NATIVE_CALL, function_name);
    }
}
