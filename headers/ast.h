#ifndef AST_H
#define AST_H
#include "token.h"
#include "error.h"
#include <stdbool.h>

// AST.H declares the abstract syntax tree of WendyScript before it compiles
//   to WendyScript Byte Code
// WendyScript Syntax Grammar is defined as follows:
//	expression_list	->	expression | expression "," expression_list
//  expression		->	or
//  or				->	and (("or") and) *
//  and				->	comparison (("and") comparison))*
//  comparison		->	term (("!=" | "==" | "<" | ">" | "<=" | ">=" | "~") term)*
//	term			->	factor (("-" | "+") factor)*
//	factor			->	unary (("*" | "/" | "\" | "%") unary)*
//	unary			->	("-" | "!" | "~") unary | member
//  member			->	array "." member | array
//  array			->	function_call | function_call ("[" expression "]")*
//  function_call	->	primary | primary ("(" expression_list ")")*
//  primary			->	NUMBER | STRING | TRUE | FALSE | NONE | IDENTIFIER |
//						"(" expression ")" | "[" expression_list "]" |
//						"#:" "(" identifier_list ")" "{" statement_list "}" 

// Inspired by https://lambda.uta.edu/cse5317/notes/node26.html
typedef struct expr {
	enum { LITERAL, BINARY, UNARY, FUNCTION, LIST, CALL } type;
	union { token											lit_expr;
			struct {	token				operator;
						struct expr*		left;
						struct expr*		right; }		bin_expr;
			struct {	token				operator;
						struct expr*		operand; }		una_expr;
			struct {	struct expr*		function;
						struct expr_list*	arguments; }	call_expr;
			struct {	struct expr_list*	contents; }		list_expr;
			struct {	struct expr_list*	parameters;
						struct statement_list* body; }		func_expr;
		} op;			
} expr;

typedef struct expr_list {
	expr*				elem;
	struct expr_list*	next;
} expr_list;

typedef struct statement { 
	enum { S_EXPR, S_OPERATION, S_LET, S_STRUCT, S_IF, S_BLOCK, S_LOOP } type;
	union { expr*										expr_statement;
			struct {	token		operator;
						expr*		operand; }			operation_statement;
			struct {	expr*		lvalue; 
						expr*		rvalue;	}			let_statement;
			struct {	token		name; 
						token		parent;
						expr_list*	instance_members;
						expr_list*  static_members; }	struct_statement;
			struct {	expr*		condition; 
						struct statement*	statement_true;
						struct statement*	statement_false; }	if_statement;
			struct {	expr*		condition;
						struct statement*	statement_true;	} loop_statement;
			struct statement_list*						block_statement;
	} op;
} statement;

typedef struct statement_list {
	statement*				elem;
	struct statement_list*	next;
} statement_list;

// is_at_end() checks if the parser is at the end
static bool is_at_end();

// match(count, ...) checks for a match of the token stream
bool fnmatch(int count, ...);

// advance() proceeds to the next token
static token advance();

// peek() peeks the next token
static token peek();

// previous() returns the last token
token previous();

// check(type) checks the next token
bool check(token_type type);

// generate_ast(tokens, length) generates an ast based on the tokens/lengths
statement_list* generate_ast(token* tokens, size_t length);

// free_ast(ast) frees all the memory associated with the AST
void free_ast(statement_list* ast);

// prints the tree in post order
void print_ast(statement_list* ast); 

// Following Functions create expression node
//   effects: will allocated memory, caller must free
expr* make_lit_expr(token t);
expr* make_bin_expr(expr* left, token op, expr* right);
expr* make_una_expr(token op, expr* operand);
expr* make_call_expr(expr* left, expr_list* arg_list);
expr* make_list_expr(expr_list* list);
expr* make_func_expr(expr_list* parameters, statement_list* body);

// The following functions create, parse, and return the corresponding
//   expression or statement.
expr* parse_expression();

statement* parse_statement();
statement_list* parse_statement_list();






#endif
