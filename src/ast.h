#ifndef AST_H
#define AST_H

#include "token.h"
#include "operators.h"
#include "codegen.h"
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
//                      "#:" "(" identifier_list ")" "{" struct statement_list "}"
// Inspired by https://lambda.uta.edu/cse5317/notes/node26.html

struct expr {
	enum { E_LITERAL, E_BINARY, E_UNARY, E_FUNCTION, E_LIST, E_CALL, E_SUPER_CALL, E_ASSIGN, E_IF, E_TABLE }
		type;
	union { struct data                                     lit_expr;
			struct {    enum vm_operator       vm_operator;
						struct expr*        left;
						struct expr*        right; }        bin_expr;
			struct {    enum vm_operator       vm_operator;
						struct expr*        operand; }      una_expr;

			/* call arguments are either resolvable to an expression or
				is an assignment expression for named arguments */
			struct {    struct expr*        function;
						struct expr_list*   arguments; }    call_expr;
			struct {
						struct expr_list*   arguments; } super_call_expr;
			struct {    int                 length;
						struct expr_list*   contents; }     list_expr;

			/* parameters are expected to be all literal identifiers, or
				assignment expression for default values */
			struct {    struct expr_list*   parameters;
						struct statement*   body;
						bool                is_native;
						char*               native_name;
						bool                is_struct_init;
						struct statement*   belongs_to_struct; }  func_expr;
			struct {	struct expr_list*   keys;
						struct expr_list* 	values; 	     } table_expr;
			struct {    struct expr*        condition;
						struct expr*        expr_true;
						struct expr*        expr_false; }   if_expr;
			struct {    enum vm_operator       vm_operator;
						struct expr*        lvalue;
						struct expr*        rvalue; }       assign_expr;

		} op;
	int col;
	int line;
};

struct expr_list {
	struct expr*               elem;
	struct expr_list*   next;
};

struct statement {
	enum { S_EXPR, S_OPERATION, S_LET, S_STRUCT, S_IF, S_BLOCK, S_LOOP,
		S_IMPORT, S_ENUM, S_BREAK, S_CONTINUE, S_BYTECODE }
		type;
	union { struct expr*                                       expr_statement;
			struct {    enum opcode        vm_operator;
						struct expr*       operand; }          operation_statement;
			struct {    char*              lvalue;
						struct expr*       rvalue; }           let_statement;
			struct {    char*              name;
						struct expr_list*  instance_members;
						struct expr_list*  static_members;
						struct expr*       init_fn;
						struct expr*       parent_struct; } struct_statement;
			struct {    struct expr*       condition;
						struct statement*  statement_true;
						struct statement*  statement_false; }  if_statement;
			struct {    char*              index_var;
						struct expr*       condition;
						struct statement*  statement_true; }   loop_statement;
			struct {    char*              name;
						struct expr_list*  values;
						struct expr*       init_fn;        }   enum_statement;
			struct statement_list*                             block_statement;
			char*                                              import_statement;
			struct {   struct token*       data;
			           size_t              size;           }   bytecode_statement;
	} op;
	int src_line;
};

struct statement_list {
	struct statement*              elem;
	struct statement_list*  next;
};

struct traversal_algorithm {
	void (*handle_expr_pre)(struct expr *, struct traversal_algorithm*);
	void (*handle_expr_list_pre)(struct expr_list *, struct traversal_algorithm*);
	void (*handle_statement_pre)(struct statement *, struct traversal_algorithm*);
	void (*handle_statement_list_pre)(struct statement_list *, struct traversal_algorithm*);

	void (*handle_expr_post)(struct expr *, struct traversal_algorithm*);
	void (*handle_expr_list_post)(struct expr_list *, struct traversal_algorithm*);
	void (*handle_statement_post)(struct statement *, struct traversal_algorithm*);
	void (*handle_statement_list_post)(struct statement_list *, struct traversal_algorithm*);

	// Internal tracker of which level of traversal
	int level;
};

#define TRAVERSAL_ALGO_PRE(a, b, c, d) { a, b, c, d, 0, 0, 0, 0, 0 }
#define TRAVERSAL_ALGO_POST(a, b, c, d) { 0, 0, 0, 0, a, b, c, d, 0 }
#define TRAVERSAL_ALGO(a, b, c, d, e, f, g, h) { a, b, c, d, e, f, g, h, 0 }

extern struct traversal_algorithm ast_safe_free_impl;

// generate_ast(tokens, length) generates an ast based on the tokens/length
struct statement_list* generate_ast(struct token* tokens, size_t length);

// free_ast(ast) frees all the memory associated with the AST module
void free_ast(struct statement_list* ast);

// print_ast(ast) prints the tree in post order
void print_ast(struct statement_list* ast);

// ast_error_flag() returns true if the AST module encountered an error, and
//   false otherwise
bool ast_error_flag(void);

void traverse_ast(struct statement_list*, struct traversal_algorithm*);
void traverse_expr(struct expr*, struct traversal_algorithm*);
void traverse_expr_list(struct expr_list*, struct traversal_algorithm*);
void traverse_statement_list(struct statement_list*, struct traversal_algorithm*);
void traverse_statement(struct statement*, struct traversal_algorithm*);

void ast_safe_free_e(struct expr* ptr, struct traversal_algorithm* algo);
void ast_safe_free_el(struct expr_list* ptr, struct traversal_algorithm* algo);
void ast_safe_free_s(struct statement* ptr, struct traversal_algorithm* algo);
void ast_safe_free_sl(struct statement_list* ptr, struct traversal_algorithm* algo);
#endif
