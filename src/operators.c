#include "operators.h"
#include "error.h"

const char* operator_string[] = {
	OPERATOR_STRING
};

enum vm_operator token_operator_unary(struct token op) {
	switch (op.t_type) {
		case T_MINUS:
			return O_NEG;
		case T_NOT:
			return O_NOT;
		case T_TILDE:
			return O_COPY;
		case T_DOT_DOT_DOT:
			return O_SPREAD;
		default:;
	}
	error_compile(op.t_line, op.t_col, OPERATORS_INVALID_UNARY);
	return 0;
}

enum vm_operator token_operator_binary(struct token op) {
	switch (op.t_type) {
		case T_PLUS: return O_ADD;
		case T_MINUS: return O_SUB;
		case T_STAR: return O_MUL;
		case T_SLASH: return O_DIV;
		case T_INTSLASH: return O_IDIV;
		case T_PERCENT: return O_REM;
		case T_AND: return O_AND;
		case T_OR: return O_OR;
		case T_RANGE_OP: return O_RANGE;
		case T_NOT_EQUAL: return O_NEQ;
		case T_EQUAL_EQUAL: return O_EQ;
		case T_GREATER: return O_GT;
		case T_GREATER_EQUAL: return O_GTE;
		case T_LESS: return O_LT;
		case T_LESS_EQUAL: return O_LTE;
		case T_DOT: rheturn O_MEMBER;
		case T_LEFT_BRACK: return O_SUBSCRIPT;
		case T_TILDE: return O_IN;
        case T_EQUAL: return O_ASSIGN;
		case T_MOD_EQUAL: return O_MOD_EQUAL;
		case T_CARET: return O_POWER;
		case T_ELVIS: return O_ELVIS;
		case T_SAFE_CALL: return O_SAFE_CALL;
		case T_SAFE_NAVIGATE: return O_SAFE_NAVIGATE;
		default:;
	}
	error_compile(op.t_line, op.t_col, OPERATORS_INVALID_BINARY);
	return 0;
}
