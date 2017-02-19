#ifndef ERROR_H
#define ERROR_H

// errors.h includes all of the error messages in WendyScript

// Scanner Messages:
#define UNTERMINATED_STRING "Unterminated string!"
#define UNEXPECTED_CHARACTER "Unexpected character!"


// Running Messages:
#define RESERVED_TOKEN "Reserved Token used for Identifier!"
#define TOKEN_DECLARED "Identifier was already declared! Use 'set' to mutate!"
#define FUNCTION_NO_START "Function start { not found."
#define RIGHT_BRACE_NO_CONSISTENT "Mismatch curly brackets }"
#define EXPECTED_END_OF_LINE "Expected end of line!"
#define COND_EVAL_NOT_BOOL "Condition must evaluate to true or false."
#define ID_NOT_FOUND "Identifier not found. Did you declare it?"
#define FUNCTION_CALL_MEM_ERROR "Function call pointer is memory out of range."
#define FUNCTION_ARG_MISMATCH "Function call has mismatching parameters!"
#define PAREN_MISMATCH "Mismatched Parentheses!"
#define NOT_NO_VAL "Expected value after ! operator."
#define NOT_REQ_BOOL "Expected Boolean value after ! operator."
#define SYNTAX_ERROR "Syntax Error!"
#define OP_SYNTAX_ERROR_BEF ": Syntax Error: Expected Value before call!"
#define OP_SYNTAX_ERROR_AFT ": Syntax Error: Expected Value after call!"
#define NUM_EXPECT_AFT_CALL ": Syntax Error: Expected Number after call!"
#define TYPE_ERROR "Type Error!"
#define MATH_DISASTER "Division by 0!"
#define EXPR_MULT_VALUE "Expression did not evaluate to single value."
#define MUTATE_CONST "Identifier is a constant and cannot be modified."
#define ARRAY_REF_NOT_NUM "Array index must be number!"
#define MEMORY_REF_ERROR "Reference to Memory Out of Range!"
#define REQ_FILE_READ_ERR "req: File read error."
#define MEMSET_NOT_NUM "Memset requires an integer address!"
#define INPUT_ERROR "Input scanning error!"

#define UNKNOWN_TOKEN ": Unknown Token!"

// error(line, message) prints an error message to the screen
void error(int line, char* message);

#endif
