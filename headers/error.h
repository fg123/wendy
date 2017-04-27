#ifndef ERROR_H
#define ERROR_H

// errors.h includes all of the error messages in WendyScript

// Wendy Messages:
#define INVALID_P_FILE "Invalid preprocessed file!"
#define REALLOC_ERROR "Internal Error: realloc failed."

// Scanner Messages:
#define UNTERMINATED_STRING "Unterminated string!"
#define UNEXPECTED_CHARACTER "Unexpected character!"
#define REQ_FILE_READ_ERR "req: File read error."

// Preprocessor Messages:
#define INCOMPLETE_LAMBDA "Incomplete lambda definition!"
#define INCOMPLETE_IF "Incomplete if statement!"
#define INCOMPLETE_STATEMENT_LIST "Incomplete statement list!"
#define INCOMPLETE_FN_CALL "Incomplete function call!"

// Interpreter Messages:
//   GENERAL ERRORS
#define RESERVED_TOKEN "Reserved Token used for Identifier!"
#define TOKEN_DECLARED "Identifier was already declared! Use 'set' to mutate!"
#define SYNTAX_ERROR "Syntax Error!"
#define TYPE_ERROR "Type Error!"
#define INCOMPLETE_STATEMENT "Incomplete statement!"
#define UNEXPECTED_TOKEN "Unexpected token!"
#define COND_EVAL_NOT_BOOL "Condition must evaluate to true or false."
#define EXPECTED_END_OF_LINE "Expected end of line!"
#define NOT_A_LIST "Referenced identifier is not a list!"
#define ID_NOT_FOUND "Identifier not found. Did you declare it?"

//   EVAL_IDENTIFIER ERRORS
#define INVALID_IDENTIFIER "Invalid identifier. Syntax error." 
#define INVALID_DEREFERENCE "Dereferencee must be Number or Address!"

//   LIST ERRORS
#define INVALID_LIST_SUBSCRIPT "List index must be a number or a range!"
#define ARRAY_REF_OUT_RANGE "Array subscript is out of range!"

//   MEMBER ERRORS
#define NOT_A_STRUCT "Can only access member of a struct or a struct instance!"
#define MEMBER_NOT_IDEN "Tried to access invalid member!"
#define MEMBER_NOT_EXIST "Requested member does not exist in class."

//   STRUCT ERRORS
#define PARENT_NOT_STRUCT "Parent is not a struct!"
#define INIT_NOT_FN "Overwritten struct init member not a function!"

//   MATH ERRORS
#define INVALID_NEGATE "Negation operand must be a Number."  
#define NUM_NUM_INVALID_OPERATOR "Invalid operator between two Numbers."
#define LIST_LIST_INVALID_OPERATOR "Invalid operator between two Lists."
#define INVALID_APPEND "Invalid operator between List and Element."
#define STRING_NUM_INVALID_OPERATOR "Invalid operator between String and Number."
#define EVAL_STACK_EMPTY "Evaluation error: stack empty!"
#define MATH_DISASTER "Division by 0!"
#define PAREN_MISMATCH "Mismatched Parentheses!"

//   MEMORY ERRORS
#define MEMORY_REF_ERROR "Reference to Memory Out of Range!"
#define STACK_OVERFLOW "Call Stack Overflow!"
#define MEMORY_OVERFLOW "Out of memory!" 
#define FUNCTION_CALL_MISMATCH "Invalid number of parameters in function call!"

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

// w_error(message) prints an error message corresponding to
//   an internal wendyscript error, not one of the user.
void w_error(char* message);

// free_error() frees the allocated space for the source
void free_error();
#endif
