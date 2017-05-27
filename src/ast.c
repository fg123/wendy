#include "ast.h"
#include "token.h"
#include "error.h"
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>

#define match(...) fnmatch(sizeof((token_type []) {__VA_ARGS__}) / sizeof(token_type), __VA_ARGS__)

token* tokens = 0;
size_t length = 0;
int index = 0;

static bool is_at_end() {
	return index == length;
}

static bool fnmatch(int count, ...) {
	va_list a_list;
    va_start(a_list, count);	
	for (int i = 0; i < count; i++) {
		token_type next = va_arg(a_list, token_type);
		if (check(next)) {
			advance();
			return true;
		}
	}
	return false;
}

static token advance() {
	if (!is_at_end()) index++;
	return previous();
}

static token previous() {
	return tokens[index - 1];
}

static token peek() {
	return tokens[index];
}

static bool check(token_type t) {
	if (is_at_end()) return false;
	return peek().t_type == t;
}

static void consume(token_type t) {
	if (check(t)) {
		advance();
	}
	else {
		error(peek().t_line, EXPECTED_TOKEN, token_string[t]);
	}
}

statement_list* generate_ast(token* _tokens, size_t _length) {
	tokens = _tokens;
	length = _length;
	index = 0;
	return parse_statement_list();
}

// Foward Declaration
expr* expression();

expr_list* identifier_list() {
	expr_list* list = malloc(sizeof(expr_list));
	list->next = 0;
	expr** curr = &list->elem;
	expr_list* curr_list = list;
	while (1) {
		if (match(IDENTIFIER)) {
			*curr = make_lit_expr(previous());
			if (match(COMMA)) {
				curr_list->next = malloc(sizeof(expr_list));
				curr_list = curr_list->next;
				curr_list->next = 0;
				curr = &curr_list->elem;
			} else break;	
		}
		else {
			error(previous().t_line, EXPECTED_IDENTIFIER);
		}
	}
	return list;
}

expr_list* expression_list() {
	expr_list* list = malloc(sizeof(expr_list));
	list->next = 0;
	expr** curr = &list->elem;
	expr_list* curr_list = list;
	while (1) {
		*curr = expression();
		if (match(COMMA)) {
			curr_list->next = malloc(sizeof(expr_list));
			curr_list = curr_list->next;
			curr_list->next = 0;
			curr = &curr_list->elem;
		} else break;
	}
	return list;
}

expr* primary() {
	if (match(STRING) || match(NUMBER) || match(TRUE) || match(FALSE) ||
		match(NONE) || match(IDENTIFIER)) {
		return make_lit_expr(previous());	
	}
	else if (match(LEFT_BRACK)) {
		expr_list* list = expression_list();
		expr* list_expr = make_list_expr(list);
		consume(RIGHT_BRACK);
		return list_expr;
	}
	else if (match(LEFT_PAREN)) {
		expr* left = expression();
		consume(RIGHT_PAREN);
		return left;
	}
	else if (match(LAMBDA)) {
		consume(LEFT_PAREN);
		expr_list* list = expression_list();
		consume(RIGHT_PAREN);
		consume(LEFT_BRACE);
		statement_list* fn_body = parse_statement_list();
		consume(RIGHT_BRACE);
		return make_func_expr(list, fn_body);
	}
	else {
		error(peek().t_line, UNEXPECTED_PRIMARY);
		return 0;
	}
}

expr* function_call() {
	expr* left = primary();
	while (match(LEFT_PAREN)) {
		expr_list* args = expression_list();
		left = make_call_expr(left, args);
		consume(RIGHT_PAREN);
	}
	return left;
}
expr* array() {
	expr* left = function_call();
	while (match(LEFT_BRACK)) {
		token op = previous();
		expr* right = expression();
		left = make_bin_expr(left, op, right);
		consume(RIGHT_BRACK);
	}
	return left;
}
expr* member() {
	expr* left = array();
	if (match(DOT)) {
		token op = previous();
		expr* right = member();	
		return make_bin_expr(left, op, right);
	}
	return left;
}
expr* unary() {
	if (match(MINUS, NOT, TILDE)) {
		token op = previous();
		expr* right = unary();
		return make_una_expr(op, right);
	}
	return member();
}
expr* factor() {
	expr* left = unary();
	while (match(STAR, SLASH, INTSLASH, PERCENT)) {
		token op = previous();
		expr* right = unary();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
expr* term() {
	expr* left = factor();
	while (match(PLUS, MINUS)) {
		token op = previous();
		expr* right = factor();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
expr* range() {
	expr* left = term();
	while (match(RANGE_OP)) {
		token op = previous();
		expr* right = term();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
expr* comparison() {
	expr* left = range();
	while (match(NOT_EQUAL, EQUAL_EQUAL, LESS, GREATER, LESS_EQUAL, 
					GREATER_EQUAL, TILDE)) {
		token op = previous();
		expr* right = range();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
expr* and() {
	expr* left = comparison();
	while (match(AND)) {
		token op = previous();
		expr* right = comparison();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
expr* or() {
	expr* left = and();
	while (match(OR)) {
		token op = previous();
		expr* right = and();
		left = make_bin_expr(left, op, right);
	}
	return left;
}
expr* expression() {
	return or();
}

statement* parse_statement() {
	token first = advance();
	statement* sm = malloc(sizeof(statement));
	switch (first.t_type) {
		case LEFT_BRACE: {
			statement_list* sl = parse_statement_list();
			consume(RIGHT_BRACE);
			sm->type = S_BLOCK;
			sm->op.block_statement = sl;
			break;
		}
		case LET: {
			// Read an expression for a LVALUE
			consume(IDENTIFIER);
			token lvalue = previous();
			expr* rvalue = 0;
			if (match(EQUAL)) {
				rvalue = expression();
			}
			else if (match(DEFFN)) {
				consume(LEFT_PAREN);
				expr_list* parameters = expression_list();
				consume(RIGHT_PAREN);
				consume(LEFT_BRACE);
				statement_list* fnbody = parse_statement_list();
				consume(RIGHT_BRACE);
				rvalue = make_func_expr(parameters, fnbody);
			}
			else {
				rvalue = make_lit_expr(none_token());
			}
			sm->type = S_LET;
			sm->op.let_statement.lvalue = lvalue;
			sm->op.let_statement.rvalue = rvalue;
			break;
		}
		case IF: {
			expr* condition = expression();
			statement* run_if_true = parse_statement();
			statement* run_if_false = 0;
			if (match(ELSE)) {
				run_if_false = parse_statement();
			}	
			sm->type = S_IF;
			sm->op.if_statement.condition = condition;
			sm->op.if_statement.statement_true = run_if_true;
			sm->op.if_statement.statement_false = run_if_false;
			break;
		}
		case LOOP: {
			expr* condition = expression();
			statement* run_if_true = parse_statement();
			sm->type = S_LOOP;
			sm->op.loop_statement.condition = condition;
			sm->op.loop_statement.statement_true = run_if_true;
			break;
		}
		case STRUCT: {
			if (!match(IDENTIFIER)) {
				error(first.t_line, SYNTAX_ERROR, "struct");	
			}
			token name = previous();
			token parent = empty_token();
			if (match(COLON)) {
				if (!match(IDENTIFIER)) {
					error(first.t_line, SYNTAX_ERROR, "struct");			
				}
				parent = previous();
			}
			consume(DEFFN);
			expr_list* static_members = 0;
			expr_list* instance_members = 0;
			while (match(LEFT_PAREN, LEFT_BRACK)) {
				if (previous().t_type == LEFT_PAREN) {
					instance_members = identifier_list();
					consume(RIGHT_PAREN);
				}
				else {
					static_members = identifier_list();
					consume(RIGHT_BRACK);
				}
			}
			sm->type = S_STRUCT;
			sm->op.struct_statement.name = name;
			sm->op.struct_statement.parent = parent;
			sm->op.struct_statement.instance_members = instance_members;
			sm->op.struct_statement.static_members = static_members;
			break;
		}
		case INC:
		case DEC:
		case INPUT:
		case EXPLODE:
		case AT:
		case REQ:
			{
			sm->type = S_OPERATION;
			sm->op.operation_statement.operator = first;
			sm->op.operation_statement.operand = expression();
			break;
		}
		case RET: {
			sm->type = S_OPERATION;
			sm->op.operation_statement.operator = first;
			if (peek().t_type != SEMICOLON && peek().t_type != RIGHT_BRACE) {
				sm->op.operation_statement.operand = expression();
			}
			else {
				sm->op.operation_statement.operand = make_lit_expr(none_token());
			}
			break;
		}
		default:
			// We advanced it so we gotta roll back.
			index--;	
			// Handle as expression.
			sm->type = S_EXPR;
			sm->op.expr_statement = expression();
			break;
	}
	match(SEMICOLON);
	return sm;
}

statement_list* parse_statement_list() {
	statement_list* ast = malloc(sizeof(statement_list));
	ast->next = 0;
	statement** curr = &ast->elem;
	statement_list* curr_ast = ast;
	while (true) {
		*curr = parse_statement();
		if (!is_at_end() && peek().t_type != RIGHT_BRACE) {
			curr_ast->next = malloc(sizeof(statement_list));
			curr_ast = curr_ast->next;
			curr_ast->next = 0;
			curr = &curr_ast->elem;
		} else break;
	}
	return ast;
}

// Expression Traversal:
/* 	void (*a)(expr*), void (*b)(expr_list*), 
	void (*c)(statement*), void (*d)(statement_list*) */
bool pre_order = 0;
int indentation = 0;

void traverse_expr(expr* expression, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));
void traverse_expr_list(expr_list* list, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));
void traverse_statement_list(statement_list* list,
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));
void traverse_statement(statement* state, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*));

void traverse_statement_list(statement_list* list,
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!list) return;
	if (pre_order) d(list);
	traverse_statement(list->elem, a, b, c, d);
	traverse_statement_list(list->next, a, b, c, d);
	if (!pre_order) d(list);
}

void traverse_statement(statement* state, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!state) return;
	if (pre_order) c(state);
	indentation++;
	if (state->type == S_LET) {
		traverse_expr(state->op.let_statement.rvalue, a, b, c, d);
	}
	else if (state->type == S_OPERATION) {
		traverse_expr(state->op.operation_statement.operand, a, b, c, d);
	}
	else if (state->type == S_EXPR) {
		traverse_expr(state->op.expr_statement, a, b, c, d);
	}
	else if (state->type == S_BLOCK) {
		traverse_statement_list(state->op.block_statement, a, b, c, d);
	}
	else if (state->type == S_STRUCT) {
		traverse_expr_list(state->op.struct_statement.instance_members, a, b, c, d);
		traverse_expr_list(state->op.struct_statement.static_members, a, b, c, d);
	}
	else if (state->type == S_IF) {
		traverse_expr(state->op.if_statement.condition, a, b, c, d);
		traverse_statement(state->op.if_statement.statement_true, a, b, c, d);
		traverse_statement(state->op.if_statement.statement_false, a, b, c, d);
	}
	else if (state->type == S_LOOP) {
		traverse_expr(state->op.loop_statement.condition, a, b, c, d);
		traverse_statement(state->op.loop_statement.statement_true, a, b, c, d);
	}
	else {
		printf("Traverse Statement: Unknown Type\n");
	}
	indentation--;
	if(!pre_order) c(state);
}

void traverse_expr(expr* expression, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!expression) return;
	if (pre_order) a(expression);
	indentation++;
	if (expression->type == E_LITERAL) {
		
	}
	else if (expression->type == E_BINARY) {
		traverse_expr(expression->op.bin_expr.left, a, b, c, d);
		traverse_expr(expression->op.bin_expr.right, a, b, c, d);
	}
	else if (expression->type == E_UNARY) {
		traverse_expr(expression->op.una_expr.operand, a, b, c, d);
	}
	else if (expression->type == E_CALL) {
		traverse_expr(expression->op.call_expr.function, a, b, c, d);
		traverse_expr_list(expression->op.call_expr.arguments, a, b, c, d);
	}
	else if (expression->type == E_LIST) {
		traverse_expr_list(expression->op.list_expr.contents, a, b, c, d);
	}
	else if (expression->type == E_FUNCTION) {
		traverse_expr_list(expression->op.func_expr.parameters, a, b, c, d);
		traverse_statement_list(expression->op.func_expr.body, a, b, c, d);
	}
	else {
		printf("Traverse Expr: Unknown Type\n");
	}
	indentation--;
	if (!pre_order) a(expression);
}

void traverse_expr_list(expr_list* list, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*)) {
	if (!list) return;
	if (pre_order) b(list);
	traverse_expr(list->elem, a, b, c, d);
	traverse_expr_list(list->next, a, b, c, d);
	if (!pre_order) b(list);
}

void print_e(void* expre) {
	printf("%*c", indentation * 2, ' ');
	expr* expression = (expr*)expre;
	if (expression->type == E_LITERAL) {
		printf("(%p) Literal Expression ", expre);
		print_token(&expression->op.lit_expr);
	}
	else if (expression->type == E_BINARY) {
		printf("(%p) Binary Expression (%p, %p)\n", expre,
			expression->op.bin_expr.left, expression->op.bin_expr.right);
	}
	else if (expression->type == E_UNARY) {
		printf("Unary Expression\n");
	}
	else if (expression->type == E_CALL) {
		printf("Call Expression\n");
	}
	else if (expression->type == E_LIST) {
		printf("List Expression\n");
	}
	else if (expression->type == E_FUNCTION) {
		printf("Function Expression\n");
	}
	else {
		printf("Traverse Expr: Unknown Type\n");
	}
}
void print_el(void* el) {
	printf("%*c", indentation * 2, ' ');
	printf("Expression List Item:\n");
}
void print_s(void* s) {
	printf("%*c", indentation * 2, ' ');
	statement* state = (statement*)s;
	if (state->type == S_LET) {
		printf("(%p) Let Statement (%s, %p)\n", state,
			state->op.let_statement.lvalue.t_data.string, 
			state->op.let_statement.rvalue);
	}
	else if (state->type == S_OPERATION) {
		printf("(%p) Operation Statement ", state);
		print_token_inline(&state->op.operation_statement.operator, stdout);
		printf(" (%p)\n", state->op.operation_statement.operand);
	}
	else if (state->type == S_EXPR) {
		printf("(%p) Expression Statement (%p)\n", state, 
			state->op.expr_statement);	
	}
	else if (state->type == S_BLOCK) {
		printf("(%p) Block Statement (%p)\n", state, state->op.block_statement);
	}
	else if (state->type == S_STRUCT) {
		printf("(%p) Struct Statement ", state);
		print_token_inline(&state->op.struct_statement.name, stdout);
		printf(":");
		print_token_inline(&state->op.struct_statement.parent, stdout);
		printf("(%p (%p)\n", state->op.struct_statement.instance_members,
			state->op.struct_statement.static_members);
	}
	else if (state->type == S_IF) {
		printf("(%p) If Statement (%p) T(%p) F(%p)\n", state,
			state->op.if_statement.condition,
			state->op.if_statement.statement_true, 
			state->op.if_statement.statement_false);
	}
	else if (state->type == S_LOOP) {
			printf("(%p) Loop Statement (%p) T(%p)\n", state, 
			state->op.loop_statement.condition,
			state->op.loop_statement.statement_true);
	}
	else {
		printf("Traverse Statement: Unknown Type\n");
	}
}
void print_sl(void* sl) {
	printf("%*c", indentation * 2, ' ');
	printf("Statement List Item:\n");
}
void print_ast(statement_list* ast) {
	pre_order = true;
	traverse_statement_list(ast, print_e, print_el, print_s, print_sl);
}

void traverse_ast(statement_list* list, 
		void (*a)(void*), void (*b)(void*), 
		void (*c)(void*), void (*d)(void*), bool order) {
	pre_order = false;
	traverse_statement_list(list, a, b, c, d);
}


void free_ast(statement_list* ast) {
	pre_order = false;
	traverse_statement_list(ast, free, free, free, free);
}

expr* make_lit_expr(token t) {
	expr* node = malloc(sizeof(expr));
	node->type = E_LITERAL;
	node->op.lit_expr = t;
	return node;
}
expr* make_bin_expr(expr* left, token op, expr* right) {
	expr* node = malloc(sizeof(expr));
	node->type = E_BINARY;
	node->op.bin_expr.operator = op;
	node->op.bin_expr.left = left;
	node->op.bin_expr.right = right;
	return node;
}
expr* make_una_expr(token op, expr* operand) {
	expr* node = malloc(sizeof(expr));
	node->type = E_UNARY;
	node->op.una_expr.operator = op;
	node->op.una_expr.operand = operand;
	return node;
}
expr* make_call_expr(expr* left, expr_list* arg_list) {
	expr* node = malloc(sizeof(expr));
	node->type = E_CALL;
	node->op.call_expr.function = left;
	node->op.call_expr.arguments = arg_list;
	return node;
}
expr* make_list_expr(expr_list* list) {
	expr* node = malloc(sizeof(expr));
	node->type = E_LIST;
	int size = 0;
	expr_list* start = list;
	while (start) {
		start = start->next;
		size++;
	}
	node->op.list_expr.length = size;
	node->op.list_expr.contents = list;
	return node;
}
expr* make_func_expr(expr_list* parameters, statement_list* body) {
	expr* node = malloc(sizeof(expr));
	node->type = E_FUNCTION;

	// Reverse Parameters
	expr_list* curr = parameters;
	expr_list* prev = 0;
	expr_list* next = 0;

	while (curr) {
		next = curr->next;
		curr->next = prev;
		prev = curr;
		curr = next;
	}
	parameters = prev;
	node->op.func_expr.parameters = parameters;
	node->op.func_expr.body = body;
	return node;
}

