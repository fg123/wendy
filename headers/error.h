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
#define TYPE_ERROR "Type Error!"
#define MATH_DISASTER "Division by 0!"
#define EXPR_MULT_VALUE "Expression did not evaluate to single value."
#define MUTATE_CONST "Identifier is a constant and cannot be modified."
#define ARRAY_REF_NOT_NUM "Array index must be number!"
#define MEMORY_REF_ERROR "Reference to Memory Out of Range!"
#define REQ_FILE_READ_ERR "req: File read error."
#define MEMSET_NOT_NUM "Memset requires an integer address!"
#define INPUT_ERROR "Input scanning error!"
#define STACK_OVERFLOW "Call Stack Overflow!"
#define MEMORY_OVERFLOW "Out of memory!" 

#define UNKNOWN_TOKEN "Unknown Token!"

// Colors
#ifdef _WIN32

#define RED   ""
#define GRN   ""
#define YEL   ""
#define BLU   ""
#define MAG   ""
#define CYN   ""
#define WHT   ""
#define RESET ""

#else

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

#endif

// init_error(src) gives the source pointer to the error module so we can
//   print the problematic line, this is called in main()
void init_error(char* src);

// error(line, message, ...) prints an error message to the screen with an
//   optional string parameter which can be appended to the front.
void error(int line, char* message, ...);

// free_error() frees the allocated space for the source
void free_error();
#endif
