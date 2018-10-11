#ifndef AST_H
#define AST_H

#include "token.h"
#include "operators.h"
#include <stdbool.h>

// ast.h - Felix Guo
// Declares the abstract syntax tree of WendyScript before it compiles
//   to WendyScript Byte Code

// WendyScript Syntax Grammar is defined as follows:
//  expression_list ->  assignment | assignment "," expression_list
//  expression      ->  assignment
//  assignment      ->  or (("=" | "=>" | "+=" | "-=" | "*=" | "/=" | "\=") or)*
//  or              ->  and (("or") and)*
//  and             ->  comparison (("and") comparison))*
//  comparison      ->  range (("in" | "!=" | "==" | "<" | ">"
//                              | "<=" | ">=" | "~") range)*
//  range           ->  comparison (("->") comparison)*
//  term            ->  factor (("-" | "+") factor)*
//  factor          ->  unary (("*" | "/" | "\" | "%") unary)*
//  unary           ->  ("-" | "!" | "~") unary | member
//  member          ->  array "." member | array
//  array           ->  function_call | function_call ("[" expression "]")*
//  function_call   ->  primary | primary ("(" expression_list ")")*
//  primary         ->  NUMBER | STRING | TRUE | FALSE | NONE | IDENTIFIER |
//                      "(" expression ")" | "[" expression_list "]" |
//                      "#:" "(" identifier_list ")" "{" statement_list "}"
// Inspired by https://lambda.uta.edu/cse5317/notes/node26.html

typedef struct expr {
	enum { E_LITERAL, E_BINARY, E_UNARY, E_FUNCTION, E_LIST, E_CALL, E_ASSIGN, E_IF }
		type;
	union { token                                           lit_expr;
			struct {    enum operator     operator;
						struct expr*        left;
						struct expr*        right; }        bin_expr;
			struct {    enum operator     operator;
						struct expr*        operand; }      una_expr;

			/* call arguments are either resolvable to an expression or
				is an assignment expression for named arguments */
			struct {    struct expr*        function;
						struct expr_list*   arguments; }    call_expr;
			struct {    int                 length;
						struct expr_list*   contents; }     list_expr;

			/* parameters are expected to be all literal identifiers, or
				assignment expression for default values */
			struct {    struct expr_list*   parameters;
						struct statement*   body;
						bool                is_native;
						token               native_name; }  func_expr;
			struct {    struct expr*        condition;
						struct expr*        expr_true;
						struct expr*        expr_false; }   if_expr;
			struct {    token               operator;
						struct expr*        lvalue;
						struct expr*        rvalue; }       assign_expr;

		} op;
	int col;
	int line;
} expr;

typedef struct expr_list {
	expr*               elem;
	struct expr_list*   next;
} expr_list;

typedef struct statement {
	enum { S_EXPR, S_OPERATION, S_LET, S_STRUCT, S_IF, S_BLOCK, S_LOOP, S_SET,
		S_IMPORT, S_EMPTY, S_BYTECODE }
		type;
	union { expr*                                       expr_statement;
			struct {    token       operator;
						expr*       operand; }          operation_statement;
			struct {    token       lvalue;
						expr*       rvalue; }           let_statement;
			struct {    token       name;
						expr_list*  instance_members;
						expr_list*  static_members;
						expr* init_fn; }                struct_statement;
			struct {    expr*       condition;
						struct statement*   statement_true;
						struct statement*   statement_false; }  if_statement;
			struct {    token       index_var;
						expr*       condition;
						struct statement*   statement_true; } loop_statement;
			struct statement_list*                      block_statement;
			token                                       import_statement;
			struct expr_list*							bytecode_statement;
	} op;
	int src_line;
} statement;

typedef struct statement_list {
	statement*              elem;
	struct statement_list*  next;
} statement_list;

typedef enum traversal_algorithm_type {
	HANDLE_BEFORE_CHILDREN, HANDLE_AFTER_CHILDREN
	} traversal_algorithm_type;

typedef struct traversal_algorithm {
	void (*handle_expr)(expr *, struct traversal_algorithm*);
	void (*handle_expr_list)(expr_list *, struct traversal_algorithm*);
	void (*handle_statement)(statement *, struct traversal_algorithm*);
	void (*handle_statement_list)(statement_list *, struct traversal_algorithm*);
	traversal_algorithm_type type;

	// Internal tracker of which level of traversal
	int level;
} traversal_algorithm;

extern traversal_algorithm ast_safe_free_impl;

// generate_ast(tokens, length) generates an ast based on the tokens/length
statement_list* generate_ast(token* tokens, size_t length);

// free_ast(ast) frees all the memory associated with the AST module
void free_ast(statement_list* ast);

// print_ast(ast) prints the tree in post order
void print_ast(statement_list* ast);

// ast_error_flag() returns true if the AST module encountered an error, and
//   false otherwise
bool ast_error_flag(void);

void traverse_ast(statement_list*, traversal_algorithm*);
void traverse_expr(expr*, traversal_algorithm*);
void traverse_expr_list(expr_list*, traversal_algorithm*);
void traverse_statement_list(statement_list*, traversal_algorithm*);
void traverse_statement(statement*, traversal_algorithm*);

#endif
