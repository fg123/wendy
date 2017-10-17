#include "codegen.h"
#include "ast.h"
#include "global.h"
#include "token.h"
#include "error.h"
#include "memory.h"
#include "execpath.h"
#include "source.h"
#include "imports.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Implementation of Wendy ByteCode Generator

static uint8_t* bytecode = 0;
static size_t capacity = 0;
static size_t size = 0;
static int global_loop_id = 0;

int verify_header(uint8_t* bytecode) {
    char* start = (char*)bytecode;
    if (strcmp(WENDY_VM_HEADER, start) == 0) {
        return strlen(WENDY_VM_HEADER) + 1;
    }
    else {
        error_general(GENERAL_INVALID_HEADER);
    }
    return 0;
}

static void guarantee_size() {
    if (size + CODEGEN_PAD_SIZE > capacity) {
        capacity *= 2;
        uint8_t* re = safe_realloc(bytecode, capacity * sizeof(uint8_t));
        bytecode = re;
    }
}

static void write_string(char* string) {
    for (int i = 0; string[i]; i++) {
        bytecode[size++] = string[i];
    }
    bytecode[size++] = 0;
    guarantee_size();
}

static void write_address(address a) {
    size_t pos = size;
    if (!is_big_endian) pos += sizeof(a);
    size += sizeof(a);
    uint8_t* first = (void*)&a;
    for (int i = 0; i < sizeof(address); i++) {
        bytecode[is_big_endian ? pos++ : --pos] = first[i];
    }
    guarantee_size();
}

static void write_address_at(address a, int pos) {
    if (!is_big_endian) pos += sizeof(address);
    uint8_t* first = (void*)&a;
    for (int i = 0; i < sizeof(address); i++) {
        bytecode[is_big_endian ? pos++ : --pos] = first[i];
    }
}

static void write_double_at(double a, int pos) {
    if (!is_big_endian) pos += sizeof(a);
    uint8_t* p = (void*)&a;
    for (int i = 0; i < sizeof(double); i++) {
        bytecode[is_big_endian ? pos++ : --pos] = p[i];
    }
}

static void write_double(double a) {
    size_t pos = size;
    if (!is_big_endian) pos += sizeof(a);
    size += sizeof(double);
    uint8_t* p = (void*)&a;
    for (int i = 0; i < sizeof(double); i++) {
        bytecode[is_big_endian ? pos++ : --pos] = p[i];
    }
    guarantee_size();
}

static void write_integer(int a) {
    write_address(a);
}

// writes the token to the bytestream, guaranteed to be a literal
static void write_token(token t) {
    bytecode[size++] = t.t_type;
    if (is_numeric(t)) {
        // Writing a double
        write_double(t.t_data.number);
    }
    else {
        write_string(t.t_data.string);
    }
    guarantee_size();
}

static void write_opcode(opcode op) {
    bytecode[size++] = op;
    guarantee_size();
}

// Generates Evaluates lValue expr and stores in memory register

static void codegen_expr(void* expre);
static void codegen_statement(void* expre);
static void codegen_statement_list(void* expre);
static void codegen_call_lvalue_expr(expr* expression);

static void codegen_lvalue_expr(expr* expression) {
    if (expression->type == E_LITERAL) {
        // Better be a identifier eh
        if (expression->op.lit_expr.t_type != T_IDENTIFIER) {
            error_lexer(expression->op.lit_expr.t_line,
                expression->op.lit_expr.t_col,
                CODEGEN_LVALUE_EXPECTED_IDENTIFIER);
        }
        write_opcode(OP_WHERE);
        write_string(expression->op.lit_expr.t_data.string);
    }
    else if (expression->type == E_BINARY) {
        // Left side in memory reg
        codegen_lvalue_expr(expression->op.bin_expr.left);

        if (expression->op.bin_expr.operator.t_type == T_DOT) {
            if (expression->op.bin_expr.right->type != E_LITERAL) {
                error_lexer(expression->line, expression->col,
                    CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL);
            }
            expression->op.bin_expr.right->op.lit_expr.t_type = T_MEMBER;
            write_opcode(OP_PUSH);
            write_token(expression->op.bin_expr.right->op.lit_expr);
            write_opcode(OP_MEMPTR);
        }
        else if (expression->op.bin_expr.operator.t_type == T_LEFT_BRACK) {
            codegen_expr(expression->op.bin_expr.right);
            write_opcode(OP_NTHPTR);
        }
        else {
            error_lexer(expression->line, expression->col,
                    CODEGEN_INVALID_LVALUE_BINOP);
        }
    }
    else if (expression->type == E_CALL) {
        codegen_expr(expression);
    }
    else {
        error_lexer(expression->line, expression->col, CODEGEN_INVALID_LVALUE);
    }
}

static void codegen_expr_list(void* expre) {
    expr_list* list = (expr_list*) expre;
    while (list) {
        codegen_expr(list->elem);
        list = list->next;
    }
}

static opcode tok_to_opcode(token t) {
    switch(t.t_type) {
        case T_INC:
            return OP_INC;
        case T_DEC:
            return OP_DEC;
        case T_INPUT:
            return OP_IN;
        case T_EXPLODE:
        case T_REQ:
        default:
            error_general(GENERAL_NOT_IMPLEMENTED, token_string[t.t_type]);
    }
    return 0;
}

static void codegen_statement(void* expre) {
    if (!expre) return;
    statement* state = (statement*) expre;

    if (!get_settings_flag(SETTINGS_COMPILE)) {
        write_opcode(OP_SRC);
        write_integer(state->src_line);
    }
    if (state->type == S_LET) {
        codegen_expr(state->op.let_statement.rvalue);
        // Request Memory
        write_opcode(OP_RBW);
        write_string(state->op.let_statement.lvalue.t_data.string);
    }
    else if (state->type == S_OPERATION) {
        if (state->op.operation_statement.operator.t_type == T_RET) {
            codegen_expr(state->op.operation_statement.operand);
            write_opcode(OP_RET);
        }
        else if (state->op.operation_statement.operator.t_type == T_AT) {
            codegen_expr(state->op.operation_statement.operand);
            write_opcode(OP_OUTL);
        }
        else {
            codegen_lvalue_expr(state->op.operation_statement.operand);
            write_opcode(tok_to_opcode(state->op.operation_statement.operator));
        }
    }
    else if (state->type == S_EXPR) {
        codegen_expr(state->op.expr_statement);
        // Only output if it's not an assignment statement.
        if (state->op.expr_statement &&
            state->op.expr_statement->type != E_ASSIGN) write_opcode(OP_OUT);
    }
    else if (state->type == S_BLOCK) {
        codegen_statement_list(state->op.block_statement);
    }
    else if (state->type == S_IMPORT) {
        if (state->op.import_statement.t_type == T_STRING) {
            // Already handled by the scanner class.
            return;
        }
        char* library_name = state->op.import_statement.t_data.string;
        // Has to be identifier now.
        if (!has_already_imported_library(library_name)) {
            add_imported_library(library_name);
            write_opcode(OP_IMPORT);
            write_string(library_name);
            int jumpLoc = size;
            size += sizeof(address);

            char* path = get_path();
            long length = 0;
            uint8_t* buffer;
            strcat(path, "wendy-lib/");
            strcat(path, state->op.import_statement.t_data.string);
            strcat(path, ".wc");
            //printf("Attempting to open: %s\n", path);
            FILE * f = fopen(path, "r");
            if (f) {
                fseek (f, 0, SEEK_END);
                length = ftell(f);
                fseek (f, 0, SEEK_SET);
                buffer = safe_malloc(length);
                fread (buffer, sizeof(uint8_t), length, f);
                int offset = size - strlen(WENDY_VM_HEADER) - 1;
                offset_addresses(buffer, length, offset);
                for (long i = verify_header(buffer); i < length; i++) {
                    if (i == length - 1 && buffer[i] == OP_HALT) break;
                    bytecode[size++] = buffer[i];
                    guarantee_size();
                }
                safe_free(buffer);
                fclose (f);
            }
            else {
                error_lexer(state->op.import_statement.t_line,
                            state->op.import_statement.t_col,
                            CODEGEN_REQ_FILE_READ_ERR);
            }
            safe_free(path);
            write_address_at(size, jumpLoc);
        }
    }
    else if (state->type == S_STRUCT) {
        int push_size = 2; // For Header and Name
        char* struct_name = state->op.struct_statement.name.t_data.string;

        // Push Header and Name
        write_opcode(OP_PUSH);
        int metaHeaderLoc = size;
        write_token(make_token(T_STRUCT_METADATA, make_data_num(1)));
        write_opcode(OP_PUSH);
        write_token(make_token(T_STRUCT_NAME, make_data_str(struct_name)));
        write_opcode(OP_PUSH);
        write_token(make_token(T_STRUCT_STATIC, make_data_str("init")));
        // Push Init Function
        codegen_expr(state->op.struct_statement.init_fn);

        push_size += 2;

        expr_list* curr = state->op.struct_statement.instance_members;
        while (curr) {
            expr* elem = curr->elem;
            if (elem->type != E_LITERAL
                || elem->op.lit_expr.t_type != T_IDENTIFIER) {
                error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
            }
            write_opcode(OP_PUSH);
            write_token(make_token(T_STRUCT_PARAM, elem->op.lit_expr.t_data));
            push_size++;
            curr = curr->next;
        }
        curr = state->op.struct_statement.static_members;
        while (curr) {
            expr* elem = curr->elem;
            if (elem->type != E_LITERAL
                || elem->op.lit_expr.t_type != T_IDENTIFIER) {
                error_lexer(elem->line, elem->col, CODEGEN_EXPECTED_IDENTIFIER);
            }
            write_opcode(OP_PUSH);
            write_token(make_token(T_STRUCT_STATIC, elem->op.lit_expr.t_data));
            write_opcode(OP_PUSH);
            write_token(none_token());
            push_size += 2;
            curr = curr->next;
        }
        // Request Memory to Store MetaData
        write_opcode(OP_REQ);
        bytecode[size++] = push_size;
        write_opcode(OP_PLIST);
        bytecode[size++] = push_size;
        // Now there's a List Token at the top of the stack.
        write_opcode(OP_CHTYPE);
        bytecode[size++] = T_STRUCT;

        write_opcode(OP_RBW);
        write_string(struct_name);
        write_double_at(push_size, metaHeaderLoc + 1);
    }
    else if (state->type == S_IF) {
        codegen_expr(state->op.if_statement.condition);
        write_opcode(OP_JIF);
        int falseJumpLoc = size;
        size += sizeof(address);

        codegen_statement(state->op.if_statement.statement_true);
        write_opcode(OP_JMP);
        int doneJumpLoc = size;
        size += sizeof(address);
        write_address_at(size, falseJumpLoc);
        codegen_statement(state->op.if_statement.statement_false);
        write_address_at(size, doneJumpLoc);
    }
    else if (state->type == S_LOOP) {
        // Setup Loop Index
        write_opcode(OP_PUSH);
        write_token(make_token(T_NUMBER, make_data_num(0)));
        write_opcode(OP_RBW);
        char loopIndexName[30];
        sprintf(loopIndexName, ":\")%d", global_loop_id++);
        write_string(loopIndexName);

        write_opcode(OP_FRM); // Start Local Variable Frame OUTER
        if (state->op.loop_statement.index_var.t_type != T_EMPTY) {
            // User has a custom variable, also load that.
            write_opcode(OP_PUSH);
            write_token(make_token(T_NUMBER, make_data_num(0)));
            write_opcode(OP_RBW);
            write_string(state->op.loop_statement.index_var.t_data.string);
        }

        // Start of Loop, Push Condition to Stack
        int loop_start_addr = size;
        codegen_expr(state->op.loop_statement.condition);

        // Check Condition and Jump if Needed
        write_opcode(OP_LJMP);
        int loop_skip_loc = size;
        size += sizeof(address);
        write_string(loopIndexName);

        write_opcode(OP_FRM); // Start Local Variable Frame

        // Write Custom Var and Bind
        if (state->op.loop_statement.index_var.t_type != T_EMPTY) {
            write_opcode(OP_LBIND);
            write_string(state->op.loop_statement.index_var.t_data.string);
            write_string(loopIndexName);
        }
        else {
            write_opcode(OP_POP);
        }

        codegen_statement(state->op.loop_statement.statement_true);
        write_opcode(OP_END);
        write_opcode(OP_WHERE);
        write_string(loopIndexName);
        write_opcode(OP_INC);
        if (state->op.loop_statement.index_var.t_type != T_EMPTY) {
            write_opcode(OP_READ);
            write_opcode(OP_WHERE);
            write_string(state->op.loop_statement.index_var.t_data.string);
            write_opcode(OP_WRITE);
        }
        write_opcode(OP_JMP);
        write_address(loop_start_addr);

        // Write End of Loop
        write_address_at(size, loop_skip_loc);
        write_opcode(OP_END);
    }
}

static void codegen_statement_list(void* expre) {
    statement_list* list = (statement_list*) expre;
    while (list) {
        codegen_statement(list->elem);
        list = list->next;
    }
}

static void codegen_expr(void* expre) {
    if (!expre) return;
    expr* expression = (expr*)expre;
    if (expression->type == E_LITERAL) {
        // Literal Expression, we push to the stack.
        write_opcode(OP_PUSH);
        write_token(expression->op.lit_expr);
    }
    else if (expression->type == E_BINARY) {
        if (expression->op.bin_expr.operator.t_type == T_DOT) {
            codegen_expr(expression->op.bin_expr.left);
            if (expression->op.bin_expr.right->type != E_LITERAL) {
                error_lexer(expression->line, expression->col,
                    CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL);
            }
            expression->op.bin_expr.right->op.lit_expr.t_type = T_MEMBER;
            write_opcode(OP_PUSH);
            write_token(expression->op.bin_expr.right->op.lit_expr);
        }
        else {
            codegen_expr(expression->op.bin_expr.left);
            codegen_expr(expression->op.bin_expr.right);
        }
        write_opcode(OP_BIN);
        write_token(expression->op.bin_expr.operator);
    }
    else if (expression->type == E_IF) {
        codegen_expr(expression->op.if_expr.condition);
        write_opcode(OP_JIF);
        int falseJumpLoc = size;
        size += sizeof(address);
        codegen_expr(expression->op.if_expr.expr_true);
        write_opcode(OP_JMP);
        int doneJumpLoc = size;
        size += sizeof(address);
        write_address_at(size, falseJumpLoc);
        if (expression->op.if_expr.expr_false) {
            codegen_expr(expression->op.if_expr.expr_false);
        }
        else {
            write_opcode(OP_PUSH);
            write_token(none_token());
        }
        write_address_at(size, doneJumpLoc);
    }
    else if (expression->type == E_ASSIGN) {
        token operator = expression->op.assign_expr.operator;
        switch(operator.t_type) {
            case T_ASSIGN_PLUS:
                operator.t_type = T_PLUS;
                strcpy(operator.t_data.string, "+");
                break;
            case T_ASSIGN_MINUS:
                operator.t_type = T_MINUS;
                strcpy(operator.t_data.string, "-");
                break;
            case T_ASSIGN_STAR:
                operator.t_type = T_STAR;
                strcpy(operator.t_data.string, "*");
                break;
            case T_ASSIGN_SLASH:
                operator.t_type = T_SLASH;
                strcpy(operator.t_data.string, "/");
                break;
            case T_ASSIGN_INTSLASH:
                operator.t_type = T_INTSLASH;
                strcpy(operator.t_data.string, "\\");
                break;
            default: break;
        }
        codegen_expr(expression->op.assign_expr.rvalue);

        codegen_lvalue_expr(expression->op.assign_expr.lvalue);
        if (operator.t_type != T_EQUAL) {
            write_opcode(OP_READ);
            write_opcode(OP_RBIN);
            write_token(operator);
        }
        // Memory Register should still be where lvalue is
        write_opcode(OP_WRITE);
    }
    else if (expression->type == E_UNARY) {
        codegen_expr(expression->op.una_expr.operand);
//      write_opcode(tok_to_opcode(expression->op.una_expr.operator, false));
        write_opcode(OP_UNA);
        write_token(expression->op.una_expr.operator);
    }
    else if (expression->type == E_CALL) {
        codegen_expr_list(expression->op.call_expr.arguments);
        codegen_expr(expression->op.call_expr.function);
        write_opcode(OP_CALL);
    }
    else if (expression->type == E_LIST) {
        int count = expression->op.list_expr.length;
        write_opcode(OP_PUSH);
        write_token(make_token(T_LIST_HEADER, make_data_num(count)));
        expr_list* param = expression->op.list_expr.contents;
        while (param) {
            codegen_expr(param->elem);
            param = param->next;
        }
        write_opcode(OP_REQ);
        // For the Header
        bytecode[size++] = count + 1;
        write_opcode(OP_PLIST);
        bytecode[size++] = count + 1;
    }
    else if (expression->type == E_FUNCTION) {
        write_opcode(OP_JMP);
        int writeSizeLoc = size;
        size += sizeof(address);
        int startAddr = size;
        if (expression->op.func_expr.is_native) {
            write_opcode(OP_NATIVE);
            int count = 0;
            expr_list* param = expression->op.func_expr.parameters;
            while (param) {
                param = param->next;
                count++;
            }
            write_integer(count);
            write_string(expression->op.func_expr.native_name.t_data.string);
            write_opcode(OP_RET);
        }
        else {
            // Parameters are reversed from AST Generation
            expr_list* param = expression->op.func_expr.parameters;
            while (param) {
                token t = param->elem->op.lit_expr;
                write_opcode(OP_RBW);
                write_string(t.t_data.string);
                param = param->next;
            }
            if (expression->op.func_expr.body->type == S_EXPR) {
                codegen_expr(expression->op.func_expr.body->op.expr_statement);
                write_opcode(OP_RET);
            }
            else {
                codegen_statement(expression->op.func_expr.body);
                if (bytecode[size - 1] != OP_RET) {
                    // Function has no explicit Return
                    write_opcode(OP_PUSH);
                    write_token(noneret_token());
                    write_opcode(OP_RET);
                }
            }
        }
        write_address_at(size, writeSizeLoc);
        write_opcode(OP_PUSH);
        write_token(make_token(T_ADDRESS, make_data_num(startAddr)));
        write_opcode(OP_CLOSUR);
        write_opcode(OP_PUSH);
        write_token(make_token(T_STRING, make_data_str("self")));
        write_opcode(OP_REQ);
        bytecode[size++] = 3;
        write_opcode(OP_PLIST);
        bytecode[size++] = 3;
        write_opcode(OP_CHTYPE);
        bytecode[size++] = T_FUNCTION;
    }
}

uint8_t* generate_code(statement_list* _ast, size_t* size_ptr) {
    capacity = CODEGEN_START_SIZE;
    init_imported_libraries_ll();
    bytecode = safe_malloc(capacity * sizeof(uint8_t));
    size = 0;
    if (!get_settings_flag(SETTINGS_REPL)) {
        write_string(WENDY_VM_HEADER);
    }
    codegen_statement_list(_ast);
    write_opcode(OP_HALT);
    free_imported_libraries_ll();
    *size_ptr = size;
    return bytecode;
}

token get_token(uint8_t* bytecode, unsigned int* end) {
    // Bytecode is already Offset!
    token t;
    int start = *end;
    int i = 0;
    t.t_type = bytecode[i++];
    t.t_data.string[0] = 0;
    if (is_numeric(t)) {
        if (!is_big_endian) i += sizeof(double);
        unsigned char * p = (void*)&t.t_data.number;
        for (int j = 0; j < sizeof(double); j++) {
            p[j] = bytecode[is_big_endian ? i++ : --i];
        }
        if (!is_big_endian) i += sizeof(double);
    }
    else {
        int len = 0;
        while (bytecode[i++]) {
            t.t_data.string[len++] = bytecode[i - 1];
        }
        // i is 1 past the null term
        t.t_data.string[len++] = 0;
    }
    *end = start + i - 1;
    return t;
}

void write_bytecode(uint8_t* bytecode, FILE* buffer) {
    fwrite(bytecode, sizeof(uint8_t), size, buffer);
}

address get_address(uint8_t* bytecode) {
    // Bytecode is already offset
    int i = 0;
    if (!is_big_endian) i += sizeof(address);
    address result = 0;
    unsigned char * p = (void*)&result;
    for (int j = 0; j < sizeof(address); j++) {
        p[j] = bytecode[is_big_endian ? i++ : --i];
    }
    return result;
}

void print_bytecode(uint8_t* bytecode, FILE* buffer) {
    fprintf(buffer, RED "WendyVM ByteCode Disassembly\n" GRN ".header\n");
    fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: ", &bytecode[0], 0);
    fprintf(buffer, YEL WENDY_VM_HEADER);
    fprintf(buffer, GRN "\n.code\n");
    int maxlen = 12;
    int baseaddr = 0;
    unsigned int i = 0;
    if (!get_settings_flag(SETTINGS_REPL)) {
        i = verify_header(bytecode);
    }
    for (;; i++) {
        unsigned int start = i;
        fprintf(buffer, MAG "  <%p> " BLU "<+%04X>: " RESET, &bytecode[i], i);
        opcode op = bytecode[i];
        unsigned int p = 0;
        int printSourceLine = -1;
        p += fprintf(buffer, YEL "%6s " RESET, opcode_string[op]);
        if (op == OP_PUSH || op == OP_BIN || op == OP_UNA || op == OP_RBIN) {
            i++;
            token t = get_token(&bytecode[i], &i);
            if (t.t_type == T_STRING) {
                p += fprintf(buffer, "%.*s ", maxlen, t.t_data.string);
                if (strlen(t.t_data.string) > maxlen) {
                    p += fprintf(buffer, ">");
                }
            }
            else {
                p +=  print_token_inline(&t, buffer);
            }
        }
        else if (op == OP_BIND || op == OP_WHERE || op == OP_RBW ||
                 op == OP_IMPORT) {
            i++;
            char* c = (char*)(bytecode + i);
            p += fprintf(buffer, "%.*s ", maxlen, c);
            if (strlen(c) > maxlen) {
                p += fprintf(buffer, ">");
            }

            i += strlen(c);
            if (op == OP_IMPORT) {
                // i is at null term
                void* loc = &bytecode[i + 1];
                i += sizeof(address);
                p += fprintf(buffer, "0x%X", get_address(loc));
            }
        }
        else if (op == OP_CHTYPE) {
            i++;
            p += fprintf(buffer, "%s", token_string[bytecode[i]]);
        }
        else if (op == OP_REQ || op == OP_PLIST) {
            i++;
            p += fprintf(buffer, "0x%X", bytecode[i]);
        }
        else if (op == OP_SRC) {
            void* loc = &bytecode[i + 1];
            i += sizeof(address);
            p += fprintf(buffer, "0x%X", get_address(loc));
            printSourceLine = get_address(loc);
        }
        else if (op == OP_JMP || op == OP_JIF) {
            void* loc = &bytecode[i + 1];
            i += sizeof(address);
            p += fprintf(buffer, "0x%X", get_address(loc));
        }
        else if (op == OP_LJMP) {
            void* loc = &bytecode[i + 1];
            i += sizeof(address);
            p += fprintf(buffer, "0x%X ", get_address(loc + baseaddr));
            i++;
            char* c = (char*)(bytecode + i);
            p += fprintf(buffer, "%.*s ", maxlen, c);
            if (strlen(c) > maxlen) {
                p += fprintf(buffer, ">");
            }
            i += strlen(c);
        }
        else if (op == OP_LBIND) {
            i++;
            char* c = (char*)(bytecode + i);
            p += fprintf(buffer, "%.*s ", maxlen, c);
            i += strlen(c);
            i++;
            c = (char*)(bytecode + i);
            p += fprintf(buffer, "%.*s ", maxlen, c);
            i += strlen(c);
        }
        else if (op == OP_NATIVE) {
            i++;
            void* loc = &bytecode[i];
            i += sizeof(address);
            p += fprintf(buffer, "%d ", get_address(loc));
            char* c = (char*)(bytecode + i);
            p += fprintf(buffer, "%.*s", maxlen, c);
            i += strlen(c);
        }
        while (p++ < 30) {
            fprintf(buffer, " ");
        }
        fprintf(buffer, CYN "| ");
        for (int j = start; j <= i; j++) {
            if (j != start) fprintf(buffer, " ");
            fprintf(buffer, "%02X", bytecode[j]);
            if (j > start + 2) {
                fprintf(buffer, "... ");
                break;
            }
            else if (j == i) {
                fprintf(buffer, "    ");
                break;
            }
        }
        if (printSourceLine > 0 && !get_settings_flag(SETTINGS_REPL)) {
            printf(RESET " %s", get_source_line(printSourceLine));
        }
        fprintf(buffer, "\n" RESET);
        if (op == OP_HALT) {
            break;
        }
    }
}

static void write_address_at_buffer(address a, uint8_t* buffer, size_t loc) {
    if (!is_big_endian) loc += sizeof(address);
    unsigned char * p = (void*)&a;
    for (int i = 0; i < sizeof(address); i++) {
        buffer[is_big_endian ? loc++ : --loc] = p[i];
    }
}

static void write_token_at_buffer(token t, uint8_t* buffer, size_t loc) {
    buffer[loc++] = t.t_type;
    if (t.t_type == T_ADDRESS) {
        // Writing a double
        if (!is_big_endian) loc += sizeof(double);
        unsigned char * p = (void*)&t.t_data.number;
        for (int i = 0; i < sizeof(double); i++) {
            buffer[is_big_endian ? loc++ : --loc] = p[i];
        }
    }
}

// loops through, offsets all addresses based on import linking location
void offset_addresses(uint8_t* buffer, size_t length, int offset) {
    unsigned int i = 0;
    if (!get_settings_flag(SETTINGS_REPL)) {
        i = verify_header(buffer);
    }
    for (;; i++) {
        opcode op = buffer[i];

        if (op == OP_PUSH) {
            i++;
            size_t tokLoc = i;
            token t = get_token(&buffer[i], &i);
            //printf("%X PUSHED A TOKEN HERE WITH TYPE %d, %d\n", tokLoc - 1, t.t_type, T_ADDRESS);
            if (t.t_type == T_ADDRESS) {
                //printf("%X Token here address %X\n",i, (int)t.t_data.number);
                t.t_data.number += offset;
                write_token_at_buffer(t, buffer, tokLoc);
            }
        }
        else if (op == OP_BIN || op == OP_UNA || op == OP_RBIN) {
            i++;
            get_token(&buffer[i], &i);
        }
        else if (op == OP_BIND || op == OP_WHERE || op == OP_RBW) {
            i++;
            char* c = (char*)(buffer + i);
            i += strlen(c);
        }
        else if (op == OP_IMPORT) {
            i++;
            char* c = (char*)(buffer + i);
            i += strlen(c);
            add_imported_library(c);
            address loc = get_address(&buffer[i + 1]);
            loc += offset;
            write_address_at_buffer(loc, buffer, i + 1);
            i += sizeof(address);
        }
        else if (op == OP_SRC) {
            i += sizeof(address);
        }
        else if (op == OP_JMP || op == OP_JIF) {
            address loc = get_address(&buffer[i + 1]);
            loc += offset;
            write_address_at_buffer(loc, buffer, i + 1);
            i += sizeof(address);
        }
        else if (op == OP_LJMP) {
            address loc = get_address(&buffer[i + 1]);
            loc += offset;
            write_address_at_buffer(loc, buffer, i + 1);
            i += sizeof(address);
            i++;
            char* c = (char*)(buffer + i);
            i += strlen(c);
        }
        else if (op == OP_LBIND) {
            i++;
            char* c = (char*)(buffer + i);
            i += strlen(c);
            i++;
            c = (char*)(buffer + i);
            i += strlen(c);
        }
        else if (op == OP_ASSERT) {
            i++;
            i++;
            char* c = (char*)(buffer + i);
            i += strlen(c);
        }
        else if (op == OP_NATIVE) {
            i++;
            i += sizeof(address);
            char* c = (char*)(buffer + i);
            i += strlen(c);
        }
        else if (op == OP_CHTYPE || op == OP_REQ || op == OP_PLIST) {
            i++;
        }
        if (op == OP_HALT) {
            break;
        }
    }
}

