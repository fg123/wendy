#include "interpreter.h"
#include "token.h"
#include "scanner.h"
#include "stack.h"
#include <stdio.h>
#include <string.h>
#include "error.h"
#include "memory.h"
#include <math.h>
#include <stdbool.h>
#include <errno.h>

// tokens is a pointer to the first instruction
static token* tokens;
static size_t tokens_count;

void run(char* input_string) {
	tokens_count = scan_tokens(input_string, &tokens);
	
	bool b;
	eval_fn(0, 0, 0, 0, &b);
//	print_token(eval(tokens, size));
	

	free_error();
	free(tokens);
}

address eval_identifier(address start, size_t size) {
	// Either we have a *(MEMORY ADDR) or we have identifier[] or identifier
	// We scan until we find right brackets or right parentheses and then
	//   we track back like usual.
//	printf("EVALIDENTIFIER at");
//	print_token(&tokens[start]);
	if (size < 1) {
		error(tokens[start].t_line, SYNTAX_ERROR);
		return 0;
	}
	if (tokens[start].t_type == STAR) {
		// Evaluate the Rest and That's the Address
		token t = eval(start + 1, size - 1);
		if (t.t_type != NUMBER) {
			error(tokens[start].t_line, SYNTAX_ERROR);
			return 0;
		}
		else {
			return t.t_data.number;
		}
	}
	// Not Star, so it must be an identifier
	if (tokens[start].t_type != IDENTIFIER) {
		error(tokens[start].t_line, SYNTAX_ERROR);
		return 0;
	}
	else {
		if (size == 1) {
			return get_address_of_id(tokens[start].t_data.string, 
					tokens[start].t_line);
		}
		else if (tokens[start + 1].t_type == LEFT_BRACK && 
				tokens[start + size - 1].t_type == RIGHT_BRACK) {
			// Array
			token offset = eval(start + 2, size - 3);
			if (offset.t_type != NUMBER) {
				error(tokens[start].t_line, SYNTAX_ERROR);
				return 0;
			}
			return get_address_of_id(tokens[start].t_data.string, 
				tokens[start].t_line) + offset.t_data.number;
		}
		else {
			error(tokens[start].t_line, SYNTAX_ERROR);
			return 0;
		}
	}
}

address eval_fn(address start, size_t argc,
		char* arg_ids[], address arg_vals[], bool* has_ret) {
	// push arguments into stacks
	*has_ret = false;
	for (size_t i = 0; i < argc; i++) {
		push_stack_entry(arg_ids[i], arg_vals[i]);	
	}

	int brace_count = 0;
	int line_start = start;

	for (size_t i = start; i < tokens_count; i++) {
		token curr_t = tokens[i];
		

		if (curr_t.t_type == SEMICOLON && brace_count == 0) {
			// end of a line
			address a = 0;
			if (parse_line(line_start, (i - line_start), &a)) {
				// a return value, we pop all local automatic ones too
			//	printf("POP CUZ OF SEMI %d\n", i);
			//	print_token(&tokens[i - 1]);
				pop_frame(true); 
				*has_ret = true;
				return a;
			}
			else if (a != 0) {
				return a;
			}
			line_start = i + 1;
		}
		else if (i == tokens_count - 1 && curr_t.t_type != SEMICOLON) {
			// invalid end of line
			error(curr_t.t_line, EXPECTED_END_OF_LINE);
			return 0;
		}
		else if (curr_t.t_type == LEFT_BRACE) {
			brace_count++;
		}
		else if (curr_t.t_type == RIGHT_BRACE && brace_count == 0) {
			//printf("POP CUZ OF NTSEMI %d\n", i);
			if (pop_frame(false)) {
				*has_ret = true;
				return 0; // address to NONE
			}
			else {
				*has_ret = false;
				return 0;
			}
			
		}
		else if (curr_t.t_type == RIGHT_BRACE) {
			brace_count--;
		}
	}
	return 0;
}

bool parse_line(address start, size_t size, address* return_val) {
	// process the first character
	//printf("Parse Line: First Char:\n");
	token f_token = tokens[start];
	if (f_token.t_type == LET) {
		if (size < 2) { 
			error(f_token.t_line, SYNTAX_ERROR);
			return false; 
		}
		// LET Syntax
		//   1. LET identifier = evaluable-value;
		//   2. LET identifier; 
		//   3. LET identifier => (arg1, arg2, ...) { function block; };
		//   4. LET identifier[size];
		//   5. LET identifier[size] = evaluable-value;
		if (tokens[start + 1].t_type != IDENTIFIER) {
			// not an identifier
//			printf("NOT IDEN\n");
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		else {
			if (id_exist(tokens[start + 1].t_data.string, false)) {
				// ALREADY DECLARED
				error(f_token.t_line, TOKEN_DECLARED); 
				return false;
			}
			// check if declaration or definition
			if (size == 2) {
				push_stack_entry(tokens[start + 1].t_data.string, 
						push_memory(none_token()));	
			}
			else if (size >= 4 && tokens[start + 2].t_type == EQUAL) {
				// variable definition
				// store in memory and point
				address a = push_memory(eval(start + 3, size - 3));
				push_stack_entry(tokens[start + 1].t_data.string, a);
			}
			else if (size >= 5 && tokens[start + 2].t_type == LEFT_BRACK) {
				// is an array declaration
				// we read until the matching right bracket, then set memory
				//   spaces.
			//	printf("LEFTBRACK");
				int i = start + 3;
				int bracket_count = 0;
				while (1) {
//					print_token(&tokens[i]);
					if (tokens[i].t_type == RIGHT_BRACK && bracket_count == 0) {
						break;
					}
					else if (tokens[i].t_type == RIGHT_BRACK) {
						bracket_count--;
					}
					else if (tokens[i].t_type == LEFT_BRACK) {
						bracket_count++;
					}
					i++;
					if (i >= start + size) 
					{
						
						error(tokens[start].t_line, SYNTAX_ERROR);
						return false;
					}
				}
				token at_size = eval(start + 3, i - start - 3);
				if (at_size.t_type != NUMBER) {
					error(tokens[start].t_line, ARRAY_REF_NOT_NUM);
					return false;
				}
				// i points to the right bracket, if theres a equal
				token value = none_token();
				if (i < start + size - 1 && tokens[i + 1].t_type == EQUAL) {
					value = eval(i + 2, start + size - i - 2);
				}
				address a = push_memory(value);
				int a_size = floor(at_size.t_data.number);
//				printf("ARR DEC SIZE %d\n", a_size);
				a_size--;
				while (a_size > 0) {
					push_memory(value);
					a_size--;
				}
				push_stack_entry(tokens[start + 1].t_data.string, a);
				
			}
			else if (size >= 7 && tokens[start + 2].t_type == DEFFN) {
		
				// the bare minimum of a function definition is at least
				// let id => () {};, so it should have at least 7 tokens
				// function definition, we store the address of the next
				//   instruction: Eg:
				//   let x => (arg1, arg2, ...) { function.block; };
				//            ^-address here stored, eval fn then reads the
				//              args and assigns the variables
				// We should run through do a preliminary check that it's the 
				//   right syntax.
				if (tokens[start + 3].t_type != LEFT_PAREN || 
					tokens[start + size - 1].t_type != RIGHT_BRACE) {
					error(f_token.t_line, SYNTAX_ERROR);
				}
				else {
					// Push the address of the next instruction
					//   (left parentheses)
					address a = push_memory(
							make_token(NUMBER, make_data_num(start + 3))); 
					push_stack_entry(tokens[start + 1].t_data.string, a);
					
				}
			}
			else {
				error(f_token.t_line, SYNTAX_ERROR);
				return false;
			}
		}
	}
	else if (f_token.t_type == SET) {
		if (size < 4) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// SET Syntax
		//   1. SET identifier = evaluable-value;
		//   2. SET identifier => (arg1, arg2, ...) { function block; };
		if (tokens[start + 1].t_type != IDENTIFIER) {
			// not an identifier
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		else {
		
			// Read until = or =>
			size_t id_size = 0;
			while (tokens[start + 1 + id_size].t_type != EQUAL &&
					tokens[start + 1 + id_size].t_type != DEFFN) {
				id_size++;
				if (start + 1 + id_size == size) {
					// Can't find equal or deffn
					error(f_token.t_line, SYNTAX_ERROR);
					return false;
				}
			}
			address mem_to_mod = eval_identifier(start + 1, id_size);
	
			// check if declaration or definition
			if (tokens[start + 1 + id_size].t_type == EQUAL) {
				// replace the memory with the new value
				replace_memory(eval(start + 2 + id_size, size - 2 - id_size), 
						mem_to_mod);
			}
			else if (tokens[start + 1 + id_size].t_type == DEFFN) {
				// Same as defined above for the LET statement  
				if (tokens[start + 2 + id_size].t_type != LEFT_PAREN || 
					tokens[start + size - 1].t_type != RIGHT_BRACE) {
					error(f_token.t_line, SYNTAX_ERROR);
				}
				else {
					// Push the address of the next instruction
					//   (left parentheses)
					replace_memory(make_token(NUMBER, 
						make_data_num(start + 2 + id_size)), mem_to_mod);
				}

			}
			else {
				error(f_token.t_line, SYNTAX_ERROR);
				return false;
			}
		}
	}
	else if (f_token.t_type == MEMSET) {
		if (size < 4) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// MEMSET Syntax
		//   1. SET number = evaluable-value;
		// We need to read until the equal sign to find the address.

		int i = 0; 
		while (tokens[start + i + 1].t_type != EQUAL) {
			i++;
			if (i == size) {
				error(f_token.t_line, SYNTAX_ERROR);
				return false;
			}
		}
		token res = eval(start + 1, i);

		if (res.t_type != NUMBER) {
			// not an identifier
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		else {
			// check if declaration or definition
			// replace the memory with the new
			replace_memory(eval(start + 2 + i, size - (2 + i)), 
					res.t_data.number);
		}
	}
	else if (f_token.t_type == EXPLODE) {
		if (size != 2 || tokens[start + 1].t_type != IDENTIFIER) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// EXPLODE Syntax
		//   1. EXPLODE identifier; 
		//   Requires identifier be of string type.
		
		
		token* t = get_value_of_id(tokens[start + 1].t_data.string, 
				f_token.t_line);
		if (t->t_type != STRING) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		size_t length = strlen(t->t_data.string);
		address new_mem = memory_pointer;
		for (int i = 0; i < length; i++) {
			token c = { STRING, f_token.t_line, make_data_str("") };
			c.t_data.string[0] = t->t_data.string[i];
			c.t_data.string[1] = 0;
			push_memory(c);
		}
		push_memory(none_token());
		address stack_p = get_stack_pos_of_id(tokens[start + 1].t_data.string, 
				f_token.t_line);
		call_stack[stack_p].val = new_mem;
	}
	else if (f_token.t_type == INC) {
		if (size < 2) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// INC Syntax
		//   1. INC identifier; 
		//   Requires identifier be of number type.
	
		address mem_to_mod = eval_identifier(start + 1, size - 1);

		token* t = get_value_of_address(mem_to_mod, f_token.t_line);
		if (t->t_type != NUMBER) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		t->t_data.number++;
	}
	else if (f_token.t_type == DEC) {
		if (size < 2) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// DEC Syntax
		//   1. DEC identifier; 
		//   Requires identifier be of number type.
	
		address mem_to_mod = eval_identifier(start + 1, size - 1);

		token* t = get_value_of_address(mem_to_mod, f_token.t_line);
		if (t->t_type != NUMBER) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		t->t_data.number--;
	}
	else if (f_token.t_type == INPUT) {
		if (size < 2) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// INPUT Syntax
		//   1. INPUT identifier; 
		//   Requires identifier be of number type.
		address mem_to_mod = eval_identifier(start + 1, size - 1);
	
		// Scan one line from the input.
		char buffer[MAX_STRING_LEN];
		if (fgets(buffer, MAX_STRING_LEN, stdin)) {
				
			token* t = get_value_of_address(mem_to_mod, f_token.t_line);
			char* end_ptr = buffer;
			errno = 0;
			double d = strtod(buffer, &end_ptr);
			// null terminator or newline character
			if (errno != 0 || (*end_ptr != 0 && *end_ptr != 10)) {
				t->t_type = STRING;

				strcpy(t->t_data.string, buffer);
			}
			else {
				// conversion successful
				t->t_type = NUMBER;
				t->t_data.number = d;
			}
		}
		else {
			error(f_token.t_line, INPUT_ERROR);
			return false;
		}
	}
	else if (f_token.t_type == STRUCT || f_token.t_type == REQ) {
		return false;	
	}
	else if (f_token.t_type == IF) {
		if (size < 5) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// IF Syntax
		//   1. IF (condition) { function block };
		//   2. IF (condition) { function_block } 
		//      ELSEIF (condition) {function_block};
		//   3. IF (condition) { function_block } 
		//      ELSEIF (condition) {function_block} ...
		//      ELSE { function_block };
		
		// We scan through until the end, checking each condition as we go, and
		//   runs the first function block that satisfies, then finishes.

		// we start i at the first parentheses
		address condition_start = start + 1;
		address i = condition_start; 

		while (i < size + start) {
			while (tokens[i].t_type != LEFT_BRACE) {
				// increment until left bracket
				i++; 
				if (i >= size + start) {
					error(tokens[i].t_line, SYNTAX_ERROR);
					return false;
				}
			}
			// evaluate the result from condition_start (LEFT PAREN) to
			//   the left_bracket
			token result = eval(condition_start, i - condition_start);
			if (result.t_type != TRUE && result.t_type != FALSE) {
				error(tokens[i].t_line, COND_EVAL_NOT_BOOL);
				return false;
			}
			if (result.t_type == TRUE) {
				// this is the condition we want to eval
				push_auto_frame(); 
				bool has_ret;
				address res = eval_fn(i + 1, 0, 0, 0, &has_ret);
//				printf("IF RETURN %d\n", memory[res].t_data.number);

				if (has_ret) {
					// res stores the address of the function return value
					*return_val = res;
					return false;
				}
				else {
					// done with the line!
					return false;
				}
			}
			else {
				// it's false, lets see what's next
				int brace_count = 0;
				i++; // we move into the function
				while (i < size + start) {
					if (tokens[i].t_type == RIGHT_BRACE && brace_count == 0) {
						i++;
						break;
					}
					else if (tokens[i].t_type == RIGHT_BRACE) {
						brace_count--;
					}
					else if (tokens[i].t_type == LEFT_BRACE) {
						brace_count++;
					}
					i++;
				}
				// at this point, i is either: END OF LINE, ELSEIF. or ELSE
				if (i >= size + start) {
					return false; // done with the line
				}
				else if (tokens[i].t_type == ELSEIF) {
					i++; 
					condition_start = i;
					// move to the condition and go back up to try the next
					//   condition
				}
				else if (tokens[i].t_type == ELSE) {
					// MIGHT AS WELL EVAL THIS ONE LOL
					push_auto_frame(); 
					bool has_ret;
					address res = eval_fn(i + 2, 0, 0, 0, &has_ret);
					if (has_ret) {
						// res stores the address of the function return value
//						printf("ELSE RETURN %d\n", memory[res].t_data.number);
						*return_val = res;
						return false;
					}
					else {
						// done with the line!
						return false;
					}
				}
			}
		}
	}
	else if (f_token.t_type == PRINTSTACK) {
		print_call_stack();
	}
	else if (f_token.t_type == LOOP) {
		if (size < 5) {
			error(f_token.t_line, SYNTAX_ERROR);
			return false;
		}
		// LOOP Syntax
		//   1. LOOP (condition) { function block };
		
		// we start i at the first parentheses
		address condition_start = start + 1;
		address i = condition_start; 
	
		while (1) {
//			print_token(&tokens[i]);
			while (tokens[i].t_type != LEFT_BRACE) {
				// increment until left bracket
				i++;
//				print_token(&tokens[i]);
				if (i >= size + start) {
					error(tokens[i].t_line, SYNTAX_ERROR);
					return false;
				}
			}
//			printf("OUTOFBRACE\n");
			// evaluate the result from condition_start (LEFT PAREN) to
			//   the left_bracket
			token result = eval(condition_start, i - condition_start);
//			print_token(&result);
			if (result.t_type != TRUE && result.t_type != FALSE) {
				error(tokens[i].t_line, COND_EVAL_NOT_BOOL);
				return false;
			}
			if (result.t_type == TRUE) {
				// we run then parseline again!
				push_auto_frame(); 
				bool has_ret;
				address res = eval_fn(i + 1, 0, 0, 0, &has_ret);
//				printf("IS TRUE\n");
				if (has_ret) {
					// res stores the address of the function return value
					*return_val = res;
					return false;
				}
				else {
					// done with the line! we go back to the front and check
					//   cond again!
					i = condition_start;
				}
			}
			else {
				// condition is false
				return false;
			}
		}
	}
	else if (f_token.t_type == RET) {
	//	printf("Size: %d\n", size);
		if (size != 1) { 
			// has a return value, we set the return_val flag
			*return_val = push_memory(eval(start + 1, size - 1));
		}
		else {
			*return_val = 1;
		}
		return true;
	}
	else if (f_token.t_type == AT) {
		token print_res = eval(start + 1, size - 1);
		print_token_inline(&print_res);
		return false;
	}
	else {
		token print_res = eval(start, size);
		if (print_res.t_type != NONERET) {
			print_token(&print_res);
		}
		return false;
	}
	return false;
}

token eval_uniop(token op, token a) {
	if (op.t_type == STAR) {
		// pointer operator
		if (a.t_type != NUMBER) {
			error(a.t_line, SYNTAX_ERROR);
			return none_token();
		}
		token* c = get_value_of_address(a.t_data.number, a.t_line);
		token res = *c;
		return res;
	}
	else if (op.t_type == AMPERSAND) {
		if (a.t_type != IDENTIFIER) {
			error(a.t_line, SYNTAX_ERROR);
			return none_token();
		}
		token res = make_token(NUMBER, make_data_num(
					get_address_of_id(a.t_data.string, a.t_line)));
		return res;
	}
	else if (op.t_type == MINUS) {
		if (a.t_type != NUMBER) {
			error(a.t_line, SYNTAX_ERROR);
			return none_token();
		}
		token res = make_token(NUMBER, make_data_num(-1 * a.t_data.number));
		return res;
	}
	else if (op.t_type == NOT) {
		if (a.t_type != TRUE &&  a.t_type != FALSE) {
			error(a.t_line, SYNTAX_ERROR);
			return none_token();
		}
		return a.t_type == TRUE ? false_token() : true_token();	
	}
	else {
		// fallback case
		return none_token();
	}
}

token eval_binop(token op, token a, token b) {
	//print_token(&op);
	//print_token(&a);
	//print_token(&b);
	if (a.t_type == NUMBER && b.t_type == NUMBER) {
		switch (op.t_type) {
			case EQUAL_EQUAL: 	
				return (a.t_data.number == b.t_data.number) ? 
					true_token() : false_token();
			case NOT_EQUAL:
				return (a.t_data.number != b.t_data.number) ? 
					true_token() : false_token();
			case GREATER_EQUAL:
				return (a.t_data.number >= b.t_data.number) ? 
					true_token() : false_token();
			case GREATER:
				return (a.t_data.number > b.t_data.number) ? 
					true_token() : false_token();
			case LESS_EQUAL:
				return (a.t_data.number <= b.t_data.number) ? 
					true_token() : false_token();
			case LESS:
				return (a.t_data.number < b.t_data.number) ? 
					true_token() : false_token();
			case PLUS: 	return make_token(NUMBER, make_data_num(
									a.t_data.number + b.t_data.number));
			case MINUS:	return make_token(NUMBER, make_data_num(
									a.t_data.number - b.t_data.number));
			case STAR: 	return make_token(NUMBER, make_data_num(
									a.t_data.number * b.t_data.number));
			case SLASH:
			case INTSLASH:
			case PERCENT: 
				// check for division by zero error
				if (b.t_data.number == 0) {
					error(b.t_line, MATH_DISASTER); 
				} 
				else {
					if (op.t_type == PERCENT) {
						// modulus operator
						double a_n = a.t_data.number;
						double b_n = b.t_data.number;

						// check integer 
						if (a_n != floor(a_n) || b_n != floor(b_n)) {
							error(op.t_line, TYPE_ERROR); 
							return none_token();
						}
						else {
							return make_token(NUMBER, make_data_num(
								(long long)a.t_data.number % 
								(long long)b.t_data.number));
						}
					}
					else {
						double result = a.t_data.number / b.t_data.number;
						if (op.t_type == SLASH) {
							return make_token(NUMBER, make_data_num(result));
						}
						else {
							return make_token(NUMBER, make_data_num((int)result));
						}
					}
				}
				break;
			default: 
				error(a.t_line, TYPE_ERROR);
				break;
		}
	}
	else if((a.t_type == STRING && b.t_type == STRING) && 
			(op.t_type == EQUAL_EQUAL || op.t_type == NOT_EQUAL)) {
		return (op.t_type == EQUAL_EQUAL) ^ 
			(strcmp(a.t_data.string, b.t_data.string) == 0) ? 
			false_token() : true_token();
	}
	else if((a.t_type == NONE || b.t_type == NONE) && 
			(op.t_type == EQUAL_EQUAL || op.t_type == NOT_EQUAL)) {
		return (op.t_type == EQUAL_EQUAL) ^ 
			(a.t_type == NONE && b.t_type == NONE) ? 
			false_token() : true_token();
	}
	else if((a.t_type == STRING && b.t_type == STRING) ||
			(a.t_type == STRING && b.t_type == NUMBER) ||
			(a.t_type == NUMBER && b.t_type == STRING)) {
		if (op.t_type == PLUS) {
			// string concatenation
			if (a.t_type == NUMBER) {
				sprintf(a.t_data.string, "%g", a.t_data.number); 
			}
			if (b.t_type == NUMBER) {
				sprintf(b.t_data.string, "%g", b.t_data.number); 
			}
			size_t len1 = strlen(a.t_data.string);
			size_t len2 = strlen(b.t_data.string);
			
			//printf("A: %s SIZE: %zd\n", a.t_data.string, len1);
			//printf("B: %s SIZE: %zd\n", b.t_data.string, len2);

			char result[len1 + len2 + 1]; // add1 for the null terminator
			memcpy(result, a.t_data.string, len1); // exclude null term
			memcpy(result + len1, b.t_data.string, len2); // cat second string
			result[len1 + len2] = '\0'; // add null terminator
			return make_token(STRING, make_data_str(result));
		}
		else {
			error(a.t_line, TYPE_ERROR);
			return none_token();
		}
	}
	else if((a.t_type == TRUE || a.t_type == FALSE) &&
			(b.t_type == TRUE || b.t_type == FALSE)) {
		switch (op.t_type) {
			case AND: 	
				return (a.t_type == TRUE && b.t_type == TRUE) ? 
					true_token() : false_token();
			case OR:
				return (a.t_type == FALSE && b.t_type == FALSE) ?
					false_token() : true_token();
			default: 
				error(a.t_line, TYPE_ERROR);
				break;
		}

	}
	else {
		error(a.t_line, TYPE_ERROR);
		return none_token();
	}

	return none_token();
}

void eval_one_expr(address i, token_stack** expr_stack, token_type search_end,
		bool entire_line) {
// track back to left parentheses (or front of the tokenlist)
//	printf("EVAL ONE EXPR: \n");
//	print_token_stack(*expr_stack);
	token_stack* eval_stack = 0;
	while (!is_empty(*expr_stack) && 
			((*expr_stack)->val)->t_type != search_end) {
		token* t = (*expr_stack)->val;
		
		eval_stack = push(t, eval_stack);
		*expr_stack = pop(*expr_stack);
	}
	// Check if Left Parentheses is actually there (if not tracing from 
	// the end.
	if (is_empty(*expr_stack) && !entire_line) { 
		error(tokens[i].t_line, PAREN_MISMATCH);
	} 
	else if (!entire_line) {
		*expr_stack = pop(*expr_stack); // pop left paren
	}
	while (!is_empty(eval_stack)) {
//			printf("expr_stack is: \n");
//			print_token_stack(*expr_stack);
//			printf("\neval_stack is: \n");
//			print_token_stack(eval_stack);
//			printf("\n\n");

		token* t = eval_stack->val;
		if (t->t_type == NUMBER || t->t_type == STRING || 
			t->t_type == TRUE || t->t_type == FALSE || 
			t->t_type == NONE || t->t_type == NONERET) {
			*expr_stack = push(t, *expr_stack);
			eval_stack = pop(eval_stack);
		} 
		else {
			token op = *(eval_stack->val);
			// unary operator is either: 
			//   & > for reference (has to be unary)
			//      THIS CASE IS NOT HANDLED HERE, THIS IS HANDLED IN
			//      IDENTIFIER BEFORE WE EVEN GET INTO THIS EVAL LOOP
			//   * > for dereference
			//   - > for negation
			//   ! > for negation (has to be unary)
			bool is_unary = false;
			if (op.t_type == NOT) {
				is_unary = true;
			}
			else if (op.t_type == STAR || op.t_type == MINUS) {
				if (is_empty(*expr_stack) ||
				   ((*expr_stack)->val)->t_type != NUMBER) {
					is_unary = true;
				}
			}
			
			if (is_unary) {
				// is unary
				eval_stack = pop(eval_stack);
				token b = *(eval_stack->val);

				// result into expr
				token answer = eval_uniop(op, b);
				address adr = push_memory(answer);

				*expr_stack = push(&memory[adr], *expr_stack);
				eval_stack = pop(eval_stack);
			}
			else {
				// its an operator, we handle unary here too, for now just bin
				token a = *((*expr_stack)->val);
								// next evalstack is the other number
				eval_stack = pop(eval_stack); // remove operator
				token b = *(eval_stack->val);
				
			

				// push result into expr
				token answer = eval_binop(op, a, b);
				address adr = push_memory(answer);

		
				// remove prev number from expr_stack
				*expr_stack = pop(*expr_stack);

				*expr_stack = push(&memory[adr], *expr_stack);
				// remove next number
				eval_stack = pop(eval_stack);
			}
		}
	}
	free_stack(eval_stack);

}

token eval(address start, size_t size) {
//	print_token(&tokens[start]);
//	printf("SIZE: %zd\n", size);
	// This function assumes the tokens are simplest forms, ie, no identifiers
	token_stack* expr_stack = 0; // declare a stack
	// we actually loop until size, because we have to handle the entire string
	// if there are no parentheses
	
	for (address i = start; i <= start + size; i++) {
//		printf("evalling + %d\n", i);
		if (i == start + size) {
			eval_one_expr(i, &expr_stack, LEFT_PAREN, true);
		}
		else if (tokens[i].t_type == RIGHT_PAREN) {
			eval_one_expr(i, &expr_stack, LEFT_PAREN, false);
		}
		else if (tokens[i].t_type == IDENTIFIER) {
			if (i < start + size - 1 && 
				tokens[i + 1].t_type == LEFT_PAREN) {
				// A function call on the identifier.
				
				// We retrieve that identifier's stored, should be within the
				//   tokens block, eg identifier's stored function start 
				//   address < tokens_count
				address fn_name = i;
				token* tk = get_value_of_id(tokens[i].t_data.string,
						tokens[i].t_line);
				address fn_pointer_in_mem = 
					get_address_of_id(tokens[i].t_data.string, tokens[i].t_line);
				address fn_start = tk->t_data.number;
				if (fn_start >= tokens_count) {
					error(tokens[i].t_line, FUNCTION_CALL_MEM_ERROR);
					return none_token();
				}
			
				int req_arg_count = 0;
				// max args?
				char* arg_ids[100];
				// first we get parameter names
				// fn_start is address to \/
				//                let x => (arg1, arg2) {};
				// Go through and pull args
				fn_start++;
				while (1) {
					if (tokens[fn_start].t_type == RIGHT_PAREN) {
						break;
					}
					else if (tokens[fn_start].t_type == IDENTIFIER) {
						arg_ids[req_arg_count++] = 
							tokens[fn_start].t_data.string;
						fn_start++;
					}
					else if (tokens[fn_start].t_type == COMMA) {
						fn_start++; // skip the comma
					}
					else {
						error(tokens[fn_start].t_line, SYNTAX_ERROR);
						return none_token();
					}
				}

				// fn_start is now at the right_paren, so we add 2 to push it 
				//   to the first code instruction (skip the {)
				fn_start += 2;

				// first we pull the arguments by reading until the matching )
				//   in the function call
				address arg_vals[100];
				i += 2;
				address arg_start = i;
				address arg_eval_size = 0;
				int argc = 0;
				int brace_count = 0;
				while (1) {
					if (tokens[i].t_type == RIGHT_PAREN && arg_eval_size == 0) {
						// no arguments
						break;
					}
					else if ((tokens[i].t_type == RIGHT_PAREN || 
							 tokens[i].t_type == COMMA) && brace_count == 0) {
						// has an argument
						token arg_res = eval(arg_start, arg_eval_size);
						arg_vals[argc++] = push_memory(arg_res);
						arg_start = i + 1;
						arg_eval_size = 0;
						if (tokens[i].t_type == RIGHT_PAREN) { break; }
						i++;
					}
					else if (tokens[i].t_type == RIGHT_PAREN) {
						brace_count--;
						arg_eval_size++;
						i++;
					}
					else if (tokens[i].t_type == LEFT_PAREN) {
						brace_count++;
						arg_eval_size++;
						i++;
					}
					else {
						arg_eval_size++;
						i++;
					}
					if (i >= start + size) {
						error(tokens[i].t_line, SYNTAX_ERROR);
						return none_token();
					}
				}
				if (argc != req_arg_count) {
					printf("Requires %d but got %d!\n", req_arg_count, argc);
					error(tokens[i].t_line, FUNCTION_ARG_MISMATCH);
					return none_token();
				}
				// Now ready to run the function
			
				// push a frame for the function
				push_frame(tokens[fn_name].t_data.string);
				// add a pointer to itself so it can call itself
				push_stack_entry(tokens[fn_name].t_data.string, 
						fn_pointer_in_mem);

				bool b;
				address a = eval_fn(fn_start, argc, arg_ids, arg_vals, &b);
				
				expr_stack = push(get_value_of_address(a, tokens[i].t_line), 
						expr_stack);
			}
			else if (i < start + size - 1 && 
				tokens[i + 1].t_type == LEFT_BRACK) {
				// An array reference
				address id_address = get_address_of_id(tokens[i].t_data.string,
						tokens[i].t_line);
				
				// Now we get Offset
				int brack_count = 0;
				i += 2;
				int eval_start = i;
				while (1) {
					if (i >= start + size) { 
						error(tokens[eval_start].t_line, SYNTAX_ERROR);
						return none_token();
					}
					if (tokens[i].t_type == RIGHT_BRACK && brack_count == 0) {
						break;
					}
					else if (tokens[i].t_type == RIGHT_BRACK) {
						brack_count--;
					}
					else if (tokens[i].t_type == LEFT_BRACK) {
						brack_count++;
					}
					i++;
				}

				token res = eval(eval_start, i - eval_start);
				if (res.t_type != NUMBER) {
					error(tokens[eval_start].t_line, ARRAY_REF_NOT_NUM);
					return none_token();
				}
				int offset = floor(res.t_data.number);
				token* a = get_value_of_address(id_address + offset, 
						tokens[eval_start].t_line);
				expr_stack = push(a, expr_stack);
			}
			else {
				// Variable call.
				if (!is_empty(expr_stack) && 
						(expr_stack->val)->t_type == AMPERSAND) {
					// get address of this identifier
					token op = *(expr_stack->val);
					token answer = eval_uniop(op, tokens[i]);

					// pop the & operator
					expr_stack = pop(expr_stack);

					// push the result back into the expr stack
					expr_stack = push(&answer, expr_stack);
				}
				else {
					// retrieve value
					token* a = get_value_of_id(tokens[i].t_data.string, 
							tokens[i].t_line); 
					expr_stack = push(a, expr_stack);
				}
			}
		}
		else if (tokens[i].t_type == LEFT_PAREN && !is_empty(expr_stack) && 
				(expr_stack->val)->t_type == NUMBER) {
			// A function call on the number ptr;
						
			address fn_start = (expr_stack->val)->t_data.number;
			address fn_pointer_in_mem = (expr_stack->val)->t_data.number;
			
			expr_stack = pop(expr_stack);
			if (fn_start >= tokens_count) {
				error(tokens[i].t_line, FUNCTION_CALL_MEM_ERROR);
				return none_token();
			}
			address fn_name = fn_start - 2;
			int req_arg_count = 0;
			// max args?
			char* arg_ids[100];
			// first we get parameter names
			// fn_start is address to \/
			//                let x => (arg1, arg2) {};
			// Go through and pull args
			fn_start++;
			while (1) {
				if (tokens[fn_start].t_type == RIGHT_PAREN) {
					break;
				}
				else if (tokens[fn_start].t_type == IDENTIFIER) {
					arg_ids[req_arg_count++] = 
						tokens[fn_start].t_data.string;
					fn_start++;
				}
				else if (tokens[fn_start].t_type == COMMA) {
					fn_start++; // skip the comma
				}
				else {
					error(tokens[fn_start].t_line, SYNTAX_ERROR);
					return none_token();
				}
			}

			// fn_start is now at the right_paren, so we add 2 to push it 
			//   to the first code instruction (skip the {)
			fn_start += 2;

			// first we pull the arguments by reading until the matching )
			//   in the function call
			address arg_vals[100];
			i += 1;
			address arg_start = i;
			address arg_eval_size = 0;
			int argc = 0;
			int brace_count = 0;
			while (1) {
				if (tokens[i].t_type == RIGHT_PAREN && arg_eval_size == 0) {
					// no arguments
					break;
				}
				else if ((tokens[i].t_type == RIGHT_PAREN || 
						 tokens[i].t_type == COMMA) && brace_count == 0) {
					// has an argument
					token arg_res = eval(arg_start, arg_eval_size);
					arg_vals[argc++] = push_memory(arg_res);
					arg_start = i + 1;
					arg_eval_size = 0;
					if (tokens[i].t_type == RIGHT_PAREN) { break; }
					i++;
				}
				else if (tokens[i].t_type == RIGHT_PAREN) {
					brace_count--;
					arg_eval_size++;
					i++;
				}
				else if (tokens[i].t_type == LEFT_PAREN) {
					brace_count++;
					arg_eval_size++;
					i++;
				}
				else {
					arg_eval_size++;
					i++;
				}
				if (i >= start + size) {
					error(tokens[i].t_line, SYNTAX_ERROR);
					return none_token();
				}
			}
			if (argc != req_arg_count) {
				printf("Requires %d but got %d!\n", req_arg_count, argc);

				error(tokens[i].t_line, FUNCTION_ARG_MISMATCH);
				return none_token();
			}
			// Now ready to run the function
		
			// push a frame for the function
			push_frame(tokens[fn_name].t_data.string);
			// add a pointer to itself so it can call itself
			push_stack_entry(tokens[fn_name].t_data.string, 
					fn_pointer_in_mem);

			bool b;
			address a = eval_fn(fn_start, argc, arg_ids, arg_vals, &b); 
			expr_stack = push(get_value_of_address(a, tokens[i].t_line), 
					expr_stack);
		}
		else if (tokens[i].t_type == LAMBDA) {
			// start of a lambda function definition
			// Lambda Syntax:
			//   #:(arg1, arg2...) { fn block }
			//   This whole thing resolves to a fn pointer to the first
			//     left PAREN, as if it was defined as a function
			//   Minimum length is at least #:(){}, so 6.
			//   start + size is the end pointer.
			if (i + 5 >= start + size) {
				// Too short!
				error(tokens[i].t_line, SYNTAX_ERROR);
				return none_token();
			}
			// Syntax checking can be performed when the function is called,
			// We'll do a lambda function syntax check for the colon and the
			//   braces.
			if (tokens[++i].t_type != LEFT_PAREN) {
				error(tokens[i].t_line, SYNTAX_ERROR);
				return none_token();
			}
			address fn_start = i;
			// read until left brace
			while (1) {
				i++;
				if (i >= start + size) {
					error(tokens[i].t_line, SYNTAX_ERROR);
					return none_token();
				}
				if (tokens[i].t_type == LEFT_BRACE) {
					break;
				}
			}
			// Now we are at left brace
			int brace_count = 0;
			// standard finding matching brace
			while (1) {
				i++;
				if (i >= start + size) {
					error(tokens[i].t_line, SYNTAX_ERROR);
					return none_token();
				}
				if (tokens[i].t_type == RIGHT_BRACE && brace_count == 0) {
					break;
				}
				if (tokens[i].t_type == LEFT_BRACE) {
					brace_count++;
				}
				else if (tokens[i].t_type == RIGHT_BRACE) {
					brace_count--;
				}
			}
			// Now we are at end of lambda, we'll just push the number into expr
			// stack
			token t = make_token(NUMBER, make_data_num(fn_start));
			address t_a = push_memory(t);
			expr_stack = push(&memory[t_a], expr_stack);
//			print_token_stack(expr_stack);
		}
		else if (tokens[i].t_type == TIME) {
			address time_tok = push_memory(time_token());
			expr_stack = push(&memory[time_tok], expr_stack);
		}
		else {
			expr_stack = push(&tokens[i], expr_stack);
			//printf("pushing onto expr_stack is: ");
			//print_token(tokens[i]);
			//printf("\nResulting stack: \n");
			//print_token_stack(expr_stack);
		}
	}
//	printf("DONE EVAL LOP\n");
	// return top of the stack.
	token result = none_token();
	if (!is_empty(expr_stack)) {
		result = *(expr_stack->val);
	}
//	print_token(&result);
	free_stack(expr_stack); // frees the malloc
	return result;
}

