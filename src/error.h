#ifndef ERROR_H
#define ERROR_H

#include "token.h"
#include "memory.h"

#include <stdbool.h>

// errors.h - Felix Guo
// Includes all of the error messages in WendyScript
// There are different types of error messages. All error message display calls
//   can be provided with a formatted string and arguments, like printf.
//
// General Error: error_general(message) -> All
//   No source displayed, allocation errors, etc.
//
// Scanner/Parser Error: error_lexer(message) -> Scanner / AST
//   Source displayed with column and line number.
//
// Runtime Error: error_runtime(message) -> VM
//   Source displayed with line number, and stack frame as well.

void error_general(char* message, ...);

void error_lexer(int line, int col, char* message, ...);

// TODO: this probably needs to be synchronised, otherwise printing stuff
//   will not print properly
void error_runtime(struct memory* memory, int line, char* message, ...);

void error_compile(int line, int col, char* message, ...);

void reset_error_flag(void);

bool get_error_flag(void);

#ifdef RELEASE 
#define wendy_assert(...) 
#else
#define wendy_assert(condition, ...) assert_impl((condition), #condition, __FILE__, __LINE__, __VA_ARGS__)
#endif

void assert_impl(bool condition, const char* condition_str, const char* filename, size_t line_number, const char* message, ...);

// General Messages:
#define GENERAL_INVALID_HEADER "Invalid bytecode header!"
#define GENERAL_NOT_IMPLEMENTED "%s is not implemented yet!"

// Scanner Messages:
#define SCAN_EXPECTED_TOKEN "Syntax error! Expected token %s was not found."
#define SCAN_UNTERMINATED_STRING "Unterminated string! End string literal with `\"`."
#define SCAN_UNEXPECTED_CHARACTER "Unexpected character `%c`!"
#define SCAN_REQ_FILE_READ_ERR "File read error when trying to import."

// AST Messages:
#define AST_EXPECTED_TOKEN SCAN_EXPECTED_TOKEN
#define AST_EXPECTED_IDENTIFIER "Expected identifier in identifier list!"
#define AST_EXPECTED_PRIMARY "Expected primary expression!"
#define AST_EXPECTED_IDENTIFIER_LOOP "Expected identifier in place of loop variable."
#define AST_STRUCT_NAME_IDENTIFIER "Struct name must be an identifier!"
#define AST_STRUCT_EXPECTED_EXTENDS_IDENTIFIER "Struct extends expected identifier!"
#define AST_ENUM_NAME_IDENTIFIER "Enum name must be an identifier!"
#define AST_UNRECOGNIZED_IMPORT "Unrecognized import, expected an identifier!"
#define AST_OPERATOR_OVERLOAD_NO_OPERATOR "Expected an vm_operator!"

// Optimizer Errors
#define OPTIMIZER_NO_STATEMENT_BLOCK "Identifier declared outside of statement block!?"

// CodeGen Messages:
#define CODEGEN_LVALUE_EXPECTED_IDENTIFIER "Expected identifier in lvalue expression."
#define CODEGEN_MEMBER_ACCESS_RIGHT_NOT_LITERAL_IDENTIFIER \
	"Right parameter of member access binary vm_operator must be a valid identifier, found %s."
#define CODEGEN_NAMED_ARGUMENT_NOT_LITERAL "Named arguments must be a valid LITERAL identifier."
#define CODEGEN_UNEXPECTED_FUNCTION_PARAMETER \
	"Unexpected function parameter declaration."
#define CODEGEN_FUNCTION_DEFAULT_VALUES_AT_END \
	"Unexpected parameter with no default value!"
#define CODEGEN_INVALID_LVALUE_BINOP \
	"Invalid binary vm_operator in lvalue expression."
#define CODEGEN_INVALID_LVALUE \
	"Invalid lvalue expression!"
#define CODEGEN_NAMED_ARGUMENT_MUST_COME_AFTER_POSITIONAL \
	"Named argument must come after positional arguments."
#define CODEGEN_EXPECTED_IDENTIFIER AST_EXPECTED_IDENTIFIER
#define CODEGEN_REQ_FILE_READ_ERR SCAN_REQ_FILE_READ_ERR

#define CODEGEN_BREAK_NOT_IN_LOOP "Break statement not in loop!"
#define CODEGEN_CONTINUE_NOT_IN_LOOP "Continue statement not in loop!"

#define CODEGEN_BYTECODE_INVALID_OPCODE "Inline Bytecode: Invalid opcode '%s'"
#define CODEGEN_BYTECODE_UNEXPECTED_OPERATOR "Inline Bytecode: Unexpected vm_operator '%s'. Operators must follow BIN or UNA opcode."
#define CODEGEN_BYTECODE_UNEXPECTED_DATA_NO_CONTENT "Inline Bytecode: Found a data_type specifier but no content!"
#define CODEGEN_BYTECODE_UNEXPECTED_RAW_BYTE "Inline Bytecode: Found raw byte specifier '$' but no content!"

// Operators Messages
#define OPERATORS_INVALID_UNARY "Invalid unary vm_operator!"
#define OPERATORS_INVALID_BINARY "Invalid binary vm_operator!"

// Debugger Messages:
#define OUT_OF_BREAKPOINTS "Breakpoint limit reached! You cannot create more breakpoints!"

// MEMORY ERRORS
#define MEMORY_REF_ERROR "Reference to memory out of range!"
#define MEMORY_STACK_OVERFLOW "Internal stack overflow!"
#define MEMORY_OVERFLOW "Out of memory!"
#define MEMORY_MEM_STACK_ERROR "Cannot pop empty memory register!"
#define MEMORY_STACK_UNDERFLOW "Internal stack underflow! Did you call a function with less arguments than required?"
#define MEMORY_ID_NOT_FOUND "Identifier '%s' not found! Did you declare it?"
#define MEMORY_REGISTER_STACK_OVERFLOW "Internal memory stack overflowed."

// VM Errors:
#define VM_RET_FROM_MAIN "Illegal return statement called at the top level."
#define VM_INVALID_OPCODE "Invalid enum opcode encountered (0x%X at 0x%X)."
#define VM_VAR_DECLARED_ALREADY "Identifier '%s' was already declared!"
#define VM_NOT_A_LIST "Setting nth item of identifier must be List."
#define VM_INVALID_LVALUE_LIST_SUBSCRIPT "List index must be a number or range!"
#define VM_LIST_REF_OUT_RANGE "List subscript is out of range!"
#define VM_LIST_RANGE_ASSIGN_SIZE_MISMATCH "Size of list to assign into the range does not match! Need %d values but found %d."
#define VM_NOT_A_STRUCT "You can only access member of a struct or a struct instance!"
#define VM_NOT_A_STRUCT_MAYBE_FORGOT_RET_THIS "Tried to access member of a <noneret>, did you forget `ret this` in the init function?"
#define VM_MEMBER_NOT_EXIST "Could not evaluate member '%s' in <%s>."
#define VM_COND_EVAL_NOT_BOOL "Condition must evaluate to true or false."
#define VM_FN_CALL_NOT_FN "Initiated function call but did not find function to call."
#define VM_INVALID_LIST_SUBSCRIPT "List index must be a number or a range!"
#define VM_MEMBER_NOT_IDEN "Tried to access invalid member!"
#define VM_MATH_DISASTER "Division by 0!"
#define VM_TYPE_ERROR "Type error on vm_operator '%s'!"
#define VM_NUM_NUM_INVALID_OPERATOR "Invalid vm_operator '%s' between two numbers."
#define VM_LIST_LIST_INVALID_OPERATOR "Invalid vm_operator '%s' between two lists."
#define VM_INVALID_APPEND "Invalid vm_operator '%s' between list and element."
#define VM_STRING_NUM_INVALID_OPERATOR "Invalid vm_operator '%s' between string and number."
#define VM_STRING_STRING_INVALID_OPERATOR "Invalid vm_operator '%s' between string and string."
#define VM_INVALID_NEGATE "Negation operand must be a number."
#define VM_INVALID_NATIVE_CALL "Natively linked function '%s' not found!"
#define VM_INVALID_NATIVE_NUMBER_OF_ARGS "Natively linked function call '%s' does not match expected number of arguments!"
#define VM_INVALID_NATIVE_NUMERICAL_TYPE_ERROR "Type error in native function call. Expected numerical value."
#define VM_INVALID_NATIVE_STRING_TYPE_ERROR "Type error in native function call. Expected string value."
#define VM_STRUCT_CONSTRUCTOR_NOT_A_FUNCTION "Struct's init constructor is not a function!"
#define VM_ASSIGNING_NONERET "Attempted to use result from function that does not return a value!"
#define VM_SPREAD_NOT_ITERABLE "Spread vm_operator can only be called on List or Range."
#define VM_STRING_DUPLICATION_NEGATIVE "Attempted to multiply string with negative integer."
#define VM_LIST_DUPLICATION_NEGATIVE "Attempted to multiply list with negative integer."

#define VM_INTERNAL_ERROR "An internal VM error occured: %s"

// Colors
#if defined(_WIN32) || defined(NO_COLOR)

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

#endif
