#include "optimizer.h"
#include "ast.h"
#include "global.h"
#include "error.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// Implementation of Optimization Algorithms

// Linked list node of identifiers within a statement block
typedef struct id_node id_node;
struct id_node {
    char* id_name;
    expr* value;
    int modified_count;
    int usage_count;
    id_node* next;
};

// Statement Scope Block
typedef struct statement_block statement_block;
struct statement_block {
    id_node* id_list;
    statement_block* next;
};

static statement_block* curr_statement_block = 0;

// Forward Declarations
static void free_id_nodes(id_node* node);
static void free_statement_blocks(statement_block* block);
static statement_list* optimize_statement_list(statement_list* list);
static statement* optimize_statement(statement* state);
static expr* optimize_expr(expr* expression);
static expr_list* optimize_expr_list(expr_list* list);
static statement_list* scan_statement_list_with_new_block(statement_list* list);
static void scan_statement_list(statement_list* list);
static void scan_statement(statement* state);
static void scan_expr(expr* expression);
static void scan_expr_list(expr_list* list);
static void print_statement_blocks();

static id_node* find_id_node(char* id, int line, int col) {
    // Search in each block and each list. Returns null if not found for now.
    //   ToDo, perform identifier checking here, but will need to add in
    //     special cases like `this` for OOP
    statement_block* b = curr_statement_block;
    while (b) {
        id_node* n = b->id_list;
        while (n) {
            if (strcmp(n->id_name, id) == 0) {
                return n;
            }
            n = n->next;
        }
        b = b->next;
    }
    return 0;
}

static void remove_entry(char* id, int line, int col) {
    // Basic LL Search and Remove
    statement_block* b = curr_statement_block;
    while (b) {
        id_node* p = 0;
        id_node* n = b->id_list;
        while (n) {
            if (strcmp(n->id_name, id) == 0) {
                // remove this one
                if (p) {
                    p->next = n->next;
                }
                else {
                    b->id_list = n->next;
                }
                safe_free(n);
                return;
            }
            p = n;
            n = n->next;
        }
        b = b->next;
    }
}

static void add_usage(char* id, int line, int col) {
    id_node* r = find_id_node(id, line, col);
    if (r) r->usage_count++;
    print_statement_blocks();
}

static void remove_usage(char* id, int line, int col) {
    id_node* r = find_id_node(id, line, col);
    if (r) r->usage_count--;
    print_statement_blocks();
}

static int get_usage(char* id, int line, int col) {
    id_node* r = find_id_node(id, line, col);
    if (r) return r->usage_count;
    else return 100;
}

static expr* get_value(char* id, int line, int col) {
    id_node* r = find_id_node(id, line, col);
    if (r) return r->value;
    else return 0;
}

static int get_modified(char* id, int line, int col) {
    id_node* r = find_id_node(id, line, col);
    if (r) return r->modified_count;
    else return 100;
}

static void add_modified(char* id, int line, int col) {
    id_node* r = find_id_node(id, line, col);
    if (r) r->modified_count++;
    print_statement_blocks();
}

void optimize_safe_free(void* ptr) {
    safe_free(ptr);
}

void optimize_safe_free_remove_usage(void* expre) {
    expr* expression = (expr*)expre;
    if (expression->type == E_LITERAL 
        && expression->op.lit_expr.t_type == T_IDENTIFIER) {
        remove_usage(expression->op.lit_expr.t_data.string,
                expression->line, expression->col);
    }
    optimize_safe_free(expre);
}

statement_list* optimize_ast(statement_list* ast) {
    return scan_statement_list_with_new_block(ast);
}

static void free_statement_block(statement_block* block) {
    if (!block) return;
    free_id_nodes(curr_statement_block->id_list);
    safe_free(block);
}

static void free_id_nodes(id_node* node) {
    if (!node) return;
    free_id_nodes(node->next);
    safe_free(node);
}


static void add_node(char* id, expr* value) {
    if (!curr_statement_block) {
        error_general(OPTIMIZER_NO_STATEMENT_BLOCK);
    }
    id_node* new_node = safe_malloc(sizeof(id_node));
    new_node->id_name = id;
    new_node->value = value;
    new_node->modified_count = 0;
    new_node->usage_count = 0;
    new_node->next = curr_statement_block->id_list;
    curr_statement_block->id_list = new_node;
    print_statement_blocks();
}

static void make_new_block() {
    statement_block* new_block = safe_malloc(sizeof(statement_block));
    new_block->next = curr_statement_block;
    new_block->id_list = 0;
    curr_statement_block = new_block;
    print_statement_blocks();
}

static void delete_block() {
    statement_block* next = curr_statement_block->next;
    free_statement_block(curr_statement_block);    
    curr_statement_block = next;
    print_statement_blocks();
}

static statement_list* scan_optimize_statement_list(statement_list* list) {
    scan_statement_list(list);
    list = optimize_statement_list(list);
    list = optimize_statement_list(list);
    return list;
}
static statement_list* scan_statement_list_with_new_block(statement_list* list) {
    make_new_block();
    list = scan_optimize_statement_list(list);
    delete_block();
    return list;
}

/* OPTIMIZE CODE */
static statement* optimize_statement(statement* state) {
    if (!state) return 0;
    if (state->type == S_LET) {
        state->op.let_statement.rvalue = 
                optimize_expr(state->op.let_statement.rvalue);
        if (get_usage(state->op.let_statement.lvalue.t_data.string, 
                state->op.let_statement.lvalue.t_line,
                state->op.let_statement.lvalue.t_col) == 0 && 
            state->op.let_statement.rvalue->type == E_LITERAL) {
            remove_entry(state->op.let_statement.lvalue.t_data.string, 
                state->op.let_statement.lvalue.t_line,
                state->op.let_statement.lvalue.t_col);
            traverse_statement(state, optimize_safe_free_remove_usage, 
                optimize_safe_free, optimize_safe_free, optimize_safe_free);

            return 0;
        }
    }
    else if (state->type == S_OPERATION) {
        /*state->op.operation_statement.operand = 
            optimize_expr(state->op.operation_statement.operand);*/
    }
    else if (state->type == S_EXPR) {
        state->op.expr_statement = optimize_expr(state->op.expr_statement);
    }
    else if (state->type == S_BLOCK) {
        // Already Optmized Through Scan Stage
    }
    else if (state->type == S_STRUCT) {
        state->op.struct_statement.init_fn =
             optimize_expr(state->op.struct_statement.init_fn);
        state->op.struct_statement.instance_members = 
            optimize_expr_list(state->op.struct_statement.instance_members);
        state->op.struct_statement.static_members = 
            optimize_expr_list(state->op.struct_statement.static_members);
    }
    else if (state->type == S_IF) {
        state->op.if_statement.condition =
            optimize_expr(state->op.if_statement.condition);
        expr* condition = state->op.if_statement.condition;
        statement* run_if_false = state->op.if_statement.statement_false;
        statement* run_if_true = state->op.if_statement.statement_true;
        if (condition->type == E_LITERAL && condition->op.lit_expr.t_type == T_TRUE) {
            // Always going to be true!
            traverse_statement(run_if_false, optimize_safe_free_remove_usage, 
                optimize_safe_free, optimize_safe_free, optimize_safe_free);
            traverse_expr(condition, optimize_safe_free_remove_usage, 
                optimize_safe_free, optimize_safe_free, optimize_safe_free);
            safe_free(state);
            return run_if_true;
        }
        else if (condition->type == E_LITERAL && condition->op.lit_expr.t_type == T_FALSE) {
            traverse_statement(run_if_true, optimize_safe_free_remove_usage, 
                optimize_safe_free, optimize_safe_free, optimize_safe_free);
            traverse_expr(condition, optimize_safe_free_remove_usage, 
                optimize_safe_free, optimize_safe_free, optimize_safe_free);
            safe_free(state);
            return run_if_false;
        }
    }
    else if (state->type == S_LOOP) {
        state->op.loop_statement.condition =
            optimize_expr(state->op.loop_statement.condition);
    }
    return state;
}

static statement_list* optimize_statement_list(statement_list* list) {
    if (!list) return 0;
    list->next = optimize_statement_list(list->next);
    list->elem = optimize_statement(list->elem);
    if (list->elem) {
        return list;
    }
    else {
        // No Statement
        statement_list* next = list->next;
        safe_free(list);
        return next;
    }
}

static expr* optimize_expr(expr* expression) {
    if (!expression) return 0;
    if (expression->type == E_LITERAL) {
        if (expression->op.lit_expr.t_type == T_IDENTIFIER) {
            // Is a static, then replace with value if literal
            if (get_modified(expression->op.lit_expr.t_data.string, 
                expression->line, expression->col) == 0) {
                expr* value = get_value(expression->op.lit_expr.t_data.string, 
                    expression->line, expression->col);
                if (value && value->type == E_LITERAL) {
                    remove_usage(expression->op.lit_expr.t_data.string, 
                        expression->line, expression->col);
                    *expression = *value;
                }
            }
        }
    }
    else if (expression->type == E_BINARY || expression->type == E_BIN_LVALUE) {
        expression->op.bin_expr.left = 
            optimize_expr(expression->op.bin_expr.left);
        expression->op.bin_expr.right = 
            optimize_expr(expression->op.bin_expr.right);

        expr* left = expression->op.bin_expr.left;
        expr* right = expression->op.bin_expr.right;
        token op = expression->op.bin_expr.operator;
        if (left->type == E_LITERAL && left->op.lit_expr.t_type == T_NUMBER &&
            right->type == E_LITERAL && right->op.lit_expr.t_type == T_NUMBER) {
            // Peek Optimization is Available on Numbers
            // Optimized Reuslt will be on OP  
            bool can_optimize = true;
            double a = left->op.lit_expr.t_data.number;
            double b = right->op.lit_expr.t_data.number;
            switch (op.t_type) {
                case T_STAR:
                    op.t_data.number = a * b;
                    break;
                case T_INTSLASH:
                    if (b != 0) {
                        op.t_data.number = (int)(a / b);
                        break;
                    }
                case T_SLASH:
                    if (b != 0) {
                        op.t_data.number = a / b;
                        break;
                    }
                case T_PERCENT:
                    if (b != 0 && a == floor(a) && b == floor(b)) {
                        op.t_data.number = (long long)a % (long long)b;
                        break;
                    }
                case T_MINUS:
                    op.t_data.number = a - b;
                    break;
                case T_PLUS:
                    op.t_data.number = a + b;
                    break;
                case T_LESS:
                    op = a < b ? true_token() : false_token();
                    break;
                case T_GREATER:
                    op = a > b ? true_token() : false_token();
                    break;
                case T_LESS_EQUAL:
                    op = a <= b ? true_token() : false_token();
                    break;
                case T_GREATER_EQUAL:
                    op = a >= b ? true_token() : false_token();
                    break;
                case T_EQUAL:
                    op = a == b ? true_token() : false_token();
                    break;
                case T_NOT_EQUAL:
                    op = a != b ? true_token() : false_token();
                    break;
                default: 
                    can_optimize = false;
                    break;
            }
            if (can_optimize) {
                if (!is_boolean(op)) {
                    // Didn't get optimized to a boolean
                    op.t_type = T_NUMBER;
                }
                expression->type = E_LITERAL;
                expression->op.lit_expr = op;
                safe_free(left);
                safe_free(right);
                return expression;
            }
        }
        else if (left->type == E_LITERAL && is_boolean(left->op.lit_expr) &&
            right->type == E_LITERAL && is_boolean(right->op.lit_expr)) {
            // Peek Optimization is Available on Booleans
            bool can_optimize = true;
            bool a = left->op.lit_expr.t_type == T_TRUE;
            bool b = right->op.lit_expr.t_type == T_TRUE;
            switch (op.t_type) {
                case T_AND:
                    op = a && b ? true_token() : false_token();
                    break;
                case T_OR:
                    op = a || b ? true_token() : false_token();
                    break;
                default: 
                    can_optimize = false;
                    break;
            }
            if (can_optimize) {
                expression->type = E_LITERAL;
                expression->op.lit_expr = op;
                safe_free(left);
                safe_free(right);
                return expression;
            }
        }
    }
    else if (expression->type == E_UNARY) {
        expression->op.una_expr.operand = 
            optimize_expr(expression->op.una_expr.operand);
        token op = expression->op.una_expr.operator;
        expr* operand = expression->op.una_expr.operand;
        if (op.t_type == T_MINUS && operand->type == E_LITERAL &&
            operand->op.lit_expr.t_type == T_NUMBER) {
            // Apply here
            operand->op.lit_expr.t_data.number *= -1;
            safe_free(expression);
            return operand;
        }
        if (op.t_type == T_NOT && operand->type == E_LITERAL &&
            (operand->op.lit_expr.t_type == T_TRUE || 
            operand->op.lit_expr.t_type == T_FALSE)) {
            // Apply here
            operand->op.lit_expr.t_type = 
                operand->op.lit_expr.t_type == T_TRUE ? T_FALSE : T_TRUE;
            safe_free(expression);
            return operand;
        }
    }
    else if (expression->type == E_IF) {
        expression->op.if_expr.condition =
            optimize_expr(expression->op.if_expr.condition);
        expression->op.if_expr.expr_true = 
            optimize_expr(expression->op.if_expr.expr_true);
        expression->op.if_expr.expr_false = 
            optimize_expr(expression->op.if_expr.expr_false);
    }
    else if (expression->type == E_CALL) {
        expression->op.call_expr.function =
            optimize_expr(expression->op.call_expr.function);
        expression->op.call_expr.arguments = 
            optimize_expr_list(expression->op.call_expr.arguments);
    }
    else if (expression->type == E_LIST) {
        expression->op.list_expr.contents = 
            optimize_expr_list(expression->op.list_expr.contents);
    }
    else if (expression->type == E_FUNCTION) {
        expression->op.func_expr.parameters = 
            optimize_expr_list(expression->op.func_expr.parameters);
        expression->op.func_expr.body = 
            optimize_statement(expression->op.func_expr.body);
    }
    else if (expression->type == E_ASSIGN) {
        if (expression->op.assign_expr.lvalue->type == E_LITERAL &&
            expression->op.assign_expr.lvalue->op.lit_expr.t_type == T_IDENTIFIER) {
                // Only optimize if not lists
            if (get_usage(expression->op.assign_expr.lvalue->op.lit_expr.t_data.string,
                    expression->line, expression->col) == 0) {
                traverse_expr(expression, optimize_safe_free_remove_usage,
                    optimize_safe_free, optimize_safe_free, optimize_safe_free);
                return 0;
            }
            expression->op.assign_expr.rvalue = 
                optimize_expr(expression->op.assign_expr.rvalue);   
        }
    }
    return expression;
}

static expr_list* optimize_expr_list(expr_list* list) {
    if (!list) return 0;
    list->next = optimize_expr_list(list->next);
    list->elem = optimize_expr(list->elem);
    if (list->elem) {
        return list;
    }
    else {
        // No Statement
        expr_list* next = list->next;
        safe_free(list);
        return next;
    }
}

/* BEGIN SCANNING CODE */
static void scan_statement(statement* state) {
    if (!state) return;
    if (state->type == S_LET) {
        // Add Usage
        add_node(state->op.let_statement.lvalue.t_data.string, 
            state->op.let_statement.rvalue);
        scan_expr(state->op.let_statement.rvalue);
    }
    else if (state->type == S_OPERATION) {
        token op = state->op.operation_statement.operator;
        if (op.t_type == T_RET) {
            scan_expr(state->op.operation_statement.operand);
        }
        else if (op.t_type == T_AT) {
            scan_expr(state->op.operation_statement.operand);
        }
        else {
            add_modified(state->op.operation_statement.operand->op.lit_expr.t_data.string,
                op.t_line, op.t_col);   
        }
    }
    else if (state->type == S_EXPR) {
        scan_expr(state->op.expr_statement);
    }
    else if (state->type == S_BLOCK) {
        state->op.block_statement = 
            scan_optimize_statement_list(state->op.block_statement);
    }
    else if (state->type == S_STRUCT) {
        scan_expr(state->op.struct_statement.init_fn);
        scan_expr_list(state->op.struct_statement.instance_members);
        scan_expr_list(state->op.struct_statement.static_members);
    }
    else if (state->type == S_IF) {
        scan_expr(state->op.if_statement.condition);
        make_new_block();
        scan_statement(state->op.if_statement.statement_true);
        delete_block();
        make_new_block();
        scan_statement(state->op.if_statement.statement_false);
        delete_block();
    }
    else if (state->type == S_LOOP) {
        scan_expr(state->op.loop_statement.condition);
        make_new_block();
        scan_statement(state->op.loop_statement.statement_true);
        delete_block();
    }
}

static void scan_statement_list(statement_list* list) {
    if (!list) return;
    scan_statement(list->elem);
    scan_statement_list(list->next);
}

static void scan_expr(expr* expression) {
    if (!expression) return;
    if (expression->type == E_LITERAL) {
        if (expression->op.lit_expr.t_type == T_IDENTIFIER) {
            // Used!
            add_usage(expression->op.lit_expr.t_data.string,
                expression->line, expression->col);
        }
    }
    else if (expression->type == E_BINARY || expression->type == E_BIN_LVALUE) {
        scan_expr(expression->op.bin_expr.left);
        scan_expr(expression->op.bin_expr.right);
    }
    else if (expression->type == E_UNARY) {
        scan_expr(expression->op.una_expr.operand);
    }
    else if (expression->type == E_IF) {
        scan_expr(expression->op.if_expr.condition);
        scan_expr(expression->op.if_expr.expr_true);
        scan_expr(expression->op.if_expr.expr_false);
    }
    else if (expression->type == E_CALL) {
        scan_expr(expression->op.call_expr.function);
        scan_expr_list(expression->op.call_expr.arguments);
    }
    else if (expression->type == E_LIST) {
        scan_expr_list(expression->op.list_expr.contents);
    }
    else if (expression->type == E_FUNCTION) {
        make_new_block();
        expr_list* curr = expression->op.func_expr.parameters;
        while (curr) {
            add_node(curr->elem->op.lit_expr.t_data.string, 0);
            curr = curr->next;
        }
        scan_statement(expression->op.func_expr.body);
        delete_block();
    }
    else if (expression->type == E_ASSIGN) {
        if (expression->op.assign_expr.lvalue->type == E_LITERAL &&
            expression->op.assign_expr.lvalue->op.lit_expr.t_type == T_IDENTIFIER) {
            add_modified(expression->op.assign_expr.lvalue->op.lit_expr.t_data.string,
                expression->line, expression->col);
        }
        scan_expr(expression->op.assign_expr.lvalue);
        scan_expr(expression->op.assign_expr.rvalue);   
    }
}

static void scan_expr_list(expr_list* list) {
    if (!list) return;
    scan_expr(list->elem);
    scan_expr_list(list->next);
}

static void print_statement_blocks() {
    /*printf("STATEMENT BLOCKS \n");
    statement_block* b = curr_statement_block;
    while (b) {
        printf("Block: \n");
        id_node* n = b->id_list;
        while (n) {
            printf("    %s, modified %d, usage %d\n", n->id_name, n->modified_count, 
                n->usage_count);
            n = n->next;
        }
        b = b->next;
    }
    printf("\n");*/
}