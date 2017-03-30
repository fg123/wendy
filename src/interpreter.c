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
#include "preprocessor.h"
#include "macros.h"

// tokens is a pointer to the first instruction
static token* tokens;
static size_t tokens_count;

void run(char* input_string) {
	size_t alloc_size = 0;
	tokens_count = scan_tokens(input_string, &tokens, &alloc_size);

	tokens_count = preprocess(&tokens, tokens_count, alloc_size);

	int brace_count = 0;
	int line_start = 0;
	for (int i = 0; i < tokens_count; i++) {
		token curr_t = tokens[i];
		
		if (curr_t.t_type == SEMICOLON && brace_count == 0) {
			// end of a line
			address a = 0;
			int oldI = i;
			parse_line(line_start, (i - line_start), &i);
			if (oldI == i) { line_start = i + 1; }
			else { i--; line_start = i + 1; } 
		}
		else if (i == tokens_count - 1 && curr_t.t_type != SEMICOLON) {
			// invalid end of line
			error(curr_t.t_line, EXPECTED_END_OF_LINE);
		}
		else if (curr_t.t_type == LEFT_BRACE) {
			brace_count++;
		}
		else if (curr_t.t_type == RIGHT_BRACE && brace_count == 0) {
			if (pop_frame(false, &i)) {
				//printf("we h ere nigga\n");
				// was a function, we push a noneret
				push_arg(none_token());
			}
//			printf("i: %d\n", i);
			i--;
			line_start = i + 1;
		}
		else if (curr_t.t_type == RIGHT_BRACE) {
			brace_count--;
		}

	}
	free_error();
	free(tokens);
}

address eval_identifier(address start, size_t size) {
	// Either we have a *(MEMORY ADDR) or we have identifier[] or identifier
	// I guess we can also have a identifier[][] or any sublists.
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
		if (t.t_type != NUMBER && t.t_type != ADDRESS) {
			error(tokens[start].t_line, SYNTAX_ERROR, "*");
			return 0;
		}
		else {
			return t.t_data.number;
		}
	}
	// Not Star, so it must be an identifier
	if (size == 1) {
		if (tokens[start].t_type != IDENTIFIER) {
			error(tokens[start].t_line, SYNTAX_ERROR);
			return 0;
		}
		else {
			return get_address_of_id(tokens[start].t_data.string, 
				tokens[start].t_line);
		}
	}
	else if (tokens[start + size - 1].t_type == RIGHT_BRACK) {
		// Array sort of reference, we'll find the corresponding 
		// left bracket.
		int i = start + size - 1;
		int arr_s_size = 0;
		while (tokens[i].t_type != LEFT_BRACK) { i--; arr_s_size++; }
		// i is now the LEFT_BRACK and size is 1 more than we want
		int arr_s_start = i + 1;
		arr_s_size--;
		// Now i and arr_s_size points to the subscript. We'll collect
		//   the subscript.
		token arr_s = eval(arr_s_start, arr_s_size);
		// Now we evaluate the rest of it.
		token arr = eval(start, i - start);

//			print_token(&arr);
//			print_token(&arr_s);
		if (arr.t_type != LIST) {
			error(tokens[start].t_line, NOT_A_LIST, 
				tokens[start].t_data.string);
		}
		if (arr_s.t_type != NUMBER) {
			error(tokens[start].t_line, SYNTAX_ERROR);
			return 0;
		}

		address list_start = arr.t_data.number;
		// add1 because of header
		int offseti = floor(arr_s.t_data.number) + 1;
		
		return list_start + offseti;
	}
	else {
		error(tokens[start].t_line, SYNTAX_ERROR);
		return 0;
	}
}

bool parse_line(address start, size_t size, int* i_ptr) {
	// process the first character
	//printf("Parse Line: Size: %d\n", size);
	//print_token(&tokens[start]);
	token f_token = tokens[start];
	if (f_token.t_type == LET) {
		if (size < 2) { 
			error(f_token.t_line, SYNTAX_ERROR, "let");
			return false; 
		}
		// LET Syntax
		//   1. LET identifier = evaluable-value;
		//   2. LET identifier; 
		//   3. LET identifier => { function block; };
		//   4. LET identifier[size];
		//   5. LET identifier[size] = evaluable-value;
		if (tokens[start + 1].t_type != IDENTIFIER) {
			// not an identifier
//			printf("NOT IDEN\n");
			error(f_token.t_line, SYNTAX_ERROR, "let");
			return false;
		}
		else {
			// Check function first
			if (size >= 5 && tokens[start + 2].t_type == DEFFN) {
				if (id_exist(tokens[start + 1].t_data.string, false)) {
					// ALREADY DECLARED
					error(f_token.t_line, TOKEN_DECLARED, 
							tokens[start + 1].t_data.string); 
					return false;
				}
				// the bare minimum of a function definition is at least
				// let id => {};, so it should have at least 6 tokens
				// function definition, we store the address of the next
				//   instruction: Eg:
				//   let x => (arg1, arg2, ...) { function.block; };
				//            ^-address here stored, eval fn then reads the
				//              args and assigns the variables
				// We should run through do a preliminary check that it's the 
				//   right syntax.
				
				// Push the address of the next instruction
				//   (left parentheses)
				address a = push_memory(
						make_token(NUMBER, make_data_num(start + 4))); 
				push_stack_entry(tokens[start + 1].t_data.string, a);
			}
			else if (size >= 5 && tokens[start + 2].t_type == LEFT_BRACK) {
				if (id_exist(tokens[start + 1].t_data.string, false)) {
					// ALREADY DECLARED
					error(f_token.t_line, TOKEN_DECLARED, 
							tokens[start + 1].t_data.string); 
					return false;
				}
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
						
						error(tokens[start].t_line, SYNTAX_ERROR, "let");
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
				int a_size = floor(at_size.t_data.number);
				address a = push_memory_s(value, a_size);

				address b = push_memory(make_token(LIST, make_data_num(a)));
//				printf("TEST: %s\n", memory[a].t_data.string);	
				push_stack_entry(tokens[start + 1].t_data.string, b);
			}
			else {
				// check if declaration or definition	
				bool already_declared = 
					id_exist(tokens[start + 1].t_data.string, false);
				if (size == 2) {	
					if (already_declared) {
						return false;
					}
					else {
						push_stack_entry(tokens[start + 1].t_data.string, 
								push_memory(none_token()));	
					}
				}
				else if (size >= 4 && tokens[start + 2].t_type == EQUAL) {
					// variable definition
					// store in memory and point
					token evaled = eval(start + 3, size - 3);
					
					if (already_declared) {
						token* stored =	get_value_of_id(
								tokens[start + 1].t_data.string, 
								tokens[start].t_line);
						if (token_equal(&evaled, stored)) {
							return false;
						}
						else {
							error(f_token.t_line, TOKEN_DECLARED, 
								tokens[start + 1].t_data.string); 
							return false;
						}
					} 
					else {
						address a = push_memory(evaled);
						push_stack_entry(tokens[start + 1].t_data.string, a);
					}
				}
				
				else {
					error(f_token.t_line, SYNTAX_ERROR, "let");
					return false;
				}
			}
		}
	}
	else if (f_token.t_type == SET) {
		if (size < 4) {
			error(f_token.t_line, SYNTAX_ERROR, "set");
			return false;
		}
		// SET Syntax
		//   1. SET identifier = evaluable-value;
		//   2. SET identifier => (arg1, arg2, ...) { function block; };
				
		// Read until = or =>
		size_t id_size = 0;
		while (tokens[start + 1 + id_size].t_type != EQUAL &&
				tokens[start + 1 + id_size].t_type != DEFFN) {
			id_size++;
			if (start + 1 + id_size == size) {
				// Can't find equal or deffn
				error(f_token.t_line, SYNTAX_ERROR, "set");
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
			replace_memory(make_token(NUMBER, 
				make_data_num(start + 3 + id_size)), mem_to_mod);
		}
		else {
			error(f_token.t_line, SYNTAX_ERROR, "set");
			return false;
		}
	
	}
	else if (f_token.t_type == EXPLODE) {
		if (size != 2 || tokens[start + 1].t_type != IDENTIFIER) {
			error(f_token.t_line, SYNTAX_ERROR, "explode");
			return false;
		}
		// EXPLODE Syntax
		//   1. EXPLODE identifier; 
		//   Requires identifier be of string type.
		
		
		token* t = get_value_of_id(tokens[start + 1].t_data.string, 
				f_token.t_line);
		if (t->t_type != STRING) {
			error(f_token.t_line, SYNTAX_ERROR, "explode");
			return false;
		}
		size_t length = strlen(t->t_data.string);
		token final[length + 1];
//		address new_mem = memory_pointer;
		for (int i = 0; i < length; i++) {
			token c = { STRING, f_token.t_line, make_data_str("") };
			c.t_data.string[0] = t->t_data.string[i];
			c.t_data.string[1] = 0;
			final[i] = c;
//			push_memory(c);
		}
//		push_memory(none_token());
		address new_mem = push_memory_a(final, length);
		address new_new = push_memory(make_token(LIST, make_data_num(new_mem)));
		address stack_p = get_stack_pos_of_id(tokens[start + 1].t_data.string, 
				f_token.t_line);
		call_stack[stack_p].val = new_new;
	}
	else if (f_token.t_type == INC) {
		if (size < 2) {
			error(f_token.t_line, SYNTAX_ERROR, "inc");
			return false;
		}
		// INC Syntax
		//   1. INC identifier; 
		//   Requires identifier be of number type.
	
		address mem_to_mod = eval_identifier(start + 1, size - 1);

		token* t = get_value_of_address(mem_to_mod, f_token.t_line);
		if (t->t_type != NUMBER) {
			error(f_token.t_line, SYNTAX_ERROR, "inc");
			return false;
		}
		t->t_data.number++;
	}
	else if (f_token.t_type == DEC) {
		if (size < 2) {
			error(f_token.t_line, SYNTAX_ERROR, "dec");
			return false;
		}
		// DEC Syntax
		//   1. DEC identifier; 
		//   Requires identifier be of number type.
	
		address mem_to_mod = eval_identifier(start + 1, size - 1);

		token* t = get_value_of_address(mem_to_mod, f_token.t_line);
		if (t->t_type != NUMBER) {
			error(f_token.t_line, SYNTAX_ERROR, "dec");
			return false;
		}
		t->t_data.number--;
	}
	else if (f_token.t_type == INPUT) {
		if (size < 2) {
			error(f_token.t_line, SYNTAX_ERROR, "input");
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
				size_t len = strlen(buffer);
				// remove last newline
				buffer[len - 1] = 0;
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
		// handled by scanner
		return false;	
	}
	else if (f_token.t_type == IF) {
		if (size < 5) {
			error(f_token.t_line, SYNTAX_ERROR, "if");
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
					error(tokens[i].t_line, SYNTAX_ERROR, "if");
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
			//	printf("EVALED TURE ret pointer = %d\n", start + size + 1);

				push_auto_frame(start + size + 1, "if"); 
				*i_ptr = i + 1;
				return false;
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
					push_auto_frame(start + size + 1, "else"); 

					*i_ptr = i + 2;
					return false;
				}
			}
		}
	}
	else if (f_token.t_type == PRINTSTACK) {
		print_call_stack();
		return false;
	}
	else if (f_token.t_type == LOOP) {
		if (size < 5) {
			error(f_token.t_line, SYNTAX_ERROR, "loop");
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
					error(tokens[i].t_line, SYNTAX_ERROR, "loop");
					return false;
				}
			}
			token result = eval(condition_start, i - condition_start);
//			print_token(&result);
			if (result.t_type != TRUE && result.t_type != FALSE) {
				error(tokens[i].t_line, COND_EVAL_NOT_BOOL);
				return false;
			}
			// i points to left bracket, our return value is the start so we
			//	 can reevaluate
			if (result.t_type == TRUE) {
				// we run then parseline again!
				push_auto_frame(start, "loop"); 
				*i_ptr = i + 1;
//				printf("Shifted to *iptr: %d\n", *i_ptr);
				return false;
			}
			else {
				// condition is false
				return false;
			}
		}
	}
	else if (f_token.t_type == RET) {
		//printf("Size: %d\n", size);
		if (size != 1) { 
			
			token t = eval(start + 1, size - 1);
		//	print_token(&t);
			push_arg(t);
		}
		else {
			push_arg(noneret_token());
		}
	//	printf("Not doing\n");
		pop_frame(true, i_ptr);
//		printf("New iptr = %d\n", *i_ptr);
		return true;
	}
	else if (f_token.t_type == PUSH) {
		token t = eval(start + 1, size - 1);
//		address mem = eval_identifier(start + 1, size - 1);
/*		if (t.t_type == ARRAY_HEADER) {
			t.t_data.number = mem;
		}*/
		push_arg(t);
		return false;
	}
	else if (f_token.t_type == POP) {
		address mem = eval_identifier(start + 1, size - 1);
		token t = pop_arg();
		memory[mem] = t;
		/*if (t.t_type == ARRAY_HEADER) {
			// Previously only reserved enough for one token!
			int size = get_array_size(t);
			address new_mem = pls_give_memory(size + 1);
			// Return old memory
			here_u_go(mem, 1);
			memory[new_mem] = t;
			for (int i = 0; i < size; i++) {
				// Pop each of the array elements one by one.
				memory[new_mem + i + 1] = pop_arg();
			}
			address stack_adr = get_stack_pos_of_id(
					tokens[start + 1].t_data.string, f_token.t_line);
			call_stack[stack_adr].val = new_mem;
		}*/
		//memory[mem] = pop_arg();
		//print_token(&memory[mem]);
		return false;
	}
	else if (f_token.t_type == CALL) {
		address a = eval_identifier(start + 1, size - 1);
		token t = memory[a];
		if (t.t_type != NUMBER) {
			error(t.t_line, SYNTAX_ERROR);
		}
		int ret_val = start + size + 1; // next instruction
		push_frame(tokens[start + 1].t_data.string, ret_val);
		 
		push_stack_entry(tokens[start + 1].t_data.string, a);
		*i_ptr = t.t_data.number;
		return false;
	}
	else if (f_token.t_type == AT) {
		token print_res = eval(start + 1, size - 1);
		print_token_inline(&print_res, stdout);
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
	if (op.t_type == U_STAR) {
		// pointer operator
		if (a.t_type != NUMBER && a.t_type != ADDRESS) {
			error(a.t_line, SYNTAX_ERROR, "*");
			return none_token();
		}
		token* c = get_value_of_address(a.t_data.number, a.t_line);
		token res = *c;
		return res;
	}
	else if (op.t_type == AMPERSAND) {
		
//		token res = make_token(ADDRESS, make_data_num(
//					get_address_of_id(a.t_data.string, a.t_line)));
		return a;
	}
	else if (op.t_type == U_MINUS) {
		if (a.t_type != NUMBER) {
			error(a.t_line, SYNTAX_ERROR, "-");
			return none_token();
		}
		token res = make_token(NUMBER, make_data_num(-1 * a.t_data.number));
		return res;
	}
	else if (op.t_type == ACCESS_SIZE) {
		int size = 1;
		if (a.t_type == STRING) {
			size = strlen(a.t_data.string);
		}
		else if (a.t_type == LIST) {
			// Find list header
			address h = a.t_data.number;
			size = memory[h].t_data.number;
		}
		token res = make_token(NUMBER, make_data_num(size));
		return res;
	}
	else if (op.t_type == NOT) {
		if (a.t_type != TRUE &&  a.t_type != FALSE) {
			error(a.t_line, SYNTAX_ERROR, "!");
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
	if (op.t_type == LEFT_BRACK) {
		// Array Reference
		// A must be a list, b must be a number.
		if (a.t_type != LIST) {
			error(a.t_line, NOT_A_LIST);
		}
		// Pull list header reference from list object.
		address id_address = a.t_data.number;
		if (b.t_type != NUMBER) {
			error(b.t_line, ARRAY_REF_NOT_NUM);
			return none_token();
		}
		// Add 1 to offset because of the header.
		int offset = floor(b.t_data.number) + 1;
		token* c = get_value_of_address(id_address + offset, 
				a.t_line);
		return *c;
	}
	if (op.t_type == TYPEOF) {
		if (b.t_type != OBJ_TYPE && b.t_type != NONE) {
			error(op.t_line, TYPE_ERROR); 
		}
		if (a.t_type == NUMBER) {
			return (strcmp("Number", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == STRING) {
			return (strcmp("String", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == ADDRESS) {
			return (strcmp("Address", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == TRUE || a.t_type == FALSE) {
			return (strcmp("Bool", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == LIST) {
			return (strcmp("List", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == NONE) {
			return b.t_type == NONE ? true_token() : false_token();
		}
		else {
			return false_token();
		}
	}
	bool has_address = (a.t_type == ADDRESS || b.t_type == ADDRESS);
	if ((a.t_type == NUMBER || a.t_type == ADDRESS) && 
		(b.t_type == NUMBER || b.t_type == ADDRESS)) {
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
			case RANGE_OP:
				return range_token(a.t_data.number, b.t_data.number);
			case PLUS: 	
				return make_token(has_address ? ADDRESS : NUMBER, 
						make_data_num(a.t_data.number + b.t_data.number));
			case MINUS:	
				return make_token(has_address ? ADDRESS : NUMBER, 
						make_data_num(a.t_data.number - b.t_data.number));
			case STAR: 	
				if (has_address) error(a.t_line, TYPE_ERROR);
				return make_token(NUMBER, make_data_num(
									a.t_data.number * b.t_data.number));
			case SLASH:
			case INTSLASH:
			case PERCENT: 
				if (has_address) error(a.t_line, TYPE_ERROR);
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
							error(op.t_line, TYPE_ERROR, "/"); 
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
				error(a.t_line, TYPE_ERROR, op.t_data.string);
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
	// Done number operations: past here, addresses are numbers too.
	if (a.t_type == ADDRESS) a.t_type = NUMBER;
	if (b.t_type == ADDRESS) b.t_type = NUMBER;
	if (a.t_type == LIST || b.t_type == LIST) {
		if (a.t_type == LIST && b.t_type == LIST) {
			address start_a = a.t_data.number;
			address start_b = b.t_data.number;
			int size_a = memory[start_a].t_data.number;
			int size_b = memory[start_b].t_data.number;
	
			switch (op.t_type) {
				case EQUAL_EQUAL: {
					if (size_a != size_b) {
						return false_token();
					}
					for (int i = 0; i < size_a; i++) {
						if (!token_equal(&memory[start_a + i + 1],
										&memory[start_b + i + 1])) {
							return false_token();
						}
					}
					return true_token();

				}
				break;
				case NOT_EQUAL: {
					if (size_a != size_b) {
						return true_token();
					}
					for (int i = 0; i < size_a; i++) {
						if (token_equal(&memory[start_a + i + 1],
										&memory[start_b + i + 1])) {
							return false_token();
						}
					}
					return true_token();
				}
				break;
				case PLUS: {
					int new_size = size_a + size_b;
					token* new_list = malloc(new_size * sizeof(token));
					int n = 0;
					for (int i = 0; i < size_a; i++) {
						new_list[n++] = memory[start_a + i + 1];
					}
					for (int i = 0; i < size_b; i++) {
						new_list[n++] = memory[start_b + i + 1];
					}
					address new_adr = push_memory_a(new_list, new_size);
					free(new_list);
					return make_token(LIST, make_data_num(new_adr));
				}	
				break;
				default: error(a.t_line, SYNTAX_ERROR); break;
			}
		} // End A==List && B==List
		else if (a.t_type == LIST) {
			if (op.t_type == PLUS) {
				// list + element
				address start_a = a.t_data.number;
				int size_a = memory[start_a].t_data.number;

				token* new_list = malloc((size_a + 1) * sizeof(token));
				int n = 0;
				for (int i = 0; i < size_a; i++) {
					new_list[n++] = memory[start_a + i + 1];
				}
				new_list[n++] = b;
				address new_adr = push_memory_a(new_list, size_a + 1);
				free(new_list);
				return make_token(LIST, make_data_num(new_adr));
			}
			else { error(a.t_line, SYNTAX_ERROR); }
		}
		else if (b.t_type == LIST) {
			address start_b = b.t_data.number;
			int size_b = memory[start_b].t_data.number;

			if (op.t_type == PLUS) {
				// element + list

				token* new_list = malloc((size_b + 1) * sizeof(token));
				int n = 0;
				new_list[n++] = a;
				for (int i = 0; i < size_b; i++) {
					new_list[n++] = memory[start_b + i + 1];
				}
				address new_adr = push_memory_a(new_list, size_b + 1);
				free(new_list);
				return make_token(LIST, make_data_num(new_adr));
			}
			else if (op.t_type == TILDE) {
				// element ~ list
				for (int i = 0; i < size_b; i++) {
//					print_token(&a);
					//print_token(&memory[start_b + i + 1]);
					if (token_equal(&a, &memory[start_b + i + 1])) {
						return true_token();
					}
				}
				return false_token();
			}
			else { error(a.t_line, SYNTAX_ERROR); }
		}

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
			error(a.t_line, TYPE_ERROR, op.t_data.string);
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
				error(a.t_line, TYPE_ERROR, op.t_data.string);
				break;
		}

	}
	else {
		error(a.t_line, TYPE_ERROR, op.t_data.string);
		return none_token();
	}

	return none_token();
}

// precedence(op) returns precedence of the operator
int precedence(token op) {
	switch (op.t_type) {
		case PLUS:
		case MINUS:
			return 140;
		case STAR:
		case SLASH:
		case INTSLASH:
		case PERCENT:
			return 150;
		case AND:
			return 120;
		case OR:
			return 110;
		case RANGE_OP:
		case TYPEOF:
			return 132;
		case NOT_EQUAL:
		case EQUAL_EQUAL:
		case TILDE:
			return 130;
		case GREATER:
		case GREATER_EQUAL:
		case LESS:
		case LESS_EQUAL:
			return 130;
		case NOT:
		case U_MINUS:
		case U_STAR:
		case ACCESS_SIZE:
			return 160;
		case LEFT_BRACK:
			// Array bracket
			return 170;
		case AMPERSAND:
			// Actually precedes the left bracket!?
			return 180;
		default: 
			return 0;
	}
}

void eval_one_expr(address i, token_stack** expr_stack, token_type search_end,
		bool entire_line) {
	// This implements Shunting Yard Algorithm

//	printf("EVAL ONE EXPR: \n");
//	print_token_stack(*expr_stack);
	token_stack* eval_stack = 0;
	token_stack* operator_stack = 0;

	while (!is_empty(*expr_stack) && 
			((*expr_stack)->val)->t_type != search_end) {
		token* t = (*expr_stack)->val;

		if (precedence(*t)) {
//			printf("Hit an operator: %d\n", t->t_type);
			// is an operator
			while (operator_stack != 0 && 
				precedence(*(operator_stack->val)) > precedence(*t)) {
//				printf("IN LOOP");
				token op = *(operator_stack->val);
				operator_stack = pop(operator_stack);
				// remove operator
				
				bool is_unary = false;
				if (op.t_type == NOT || op.t_type == U_STAR || 
						op.t_type == U_MINUS || op.t_type == ACCESS_SIZE) {
					is_unary = true;
				}
	
				if (is_unary) {
					token b = *(eval_stack->val);
					token answer = eval_uniop(op, b);
					address adr = push_memory(answer);
					eval_stack = pop(eval_stack);
					eval_stack = push(&memory[adr], eval_stack);
				}
				else {
					token a = *(eval_stack->val);
					eval_stack = pop(eval_stack);
					token b = *(eval_stack->val); 
					eval_stack = pop(eval_stack);
					token answer = eval_binop(op, a, b);
					address adr = push_memory(answer);
					eval_stack = push(&memory[adr], eval_stack);
				}
			}
			operator_stack = push(t, operator_stack);
		}
		else {
			eval_stack = push(t, eval_stack);
		}
		*expr_stack = pop(*expr_stack);
	}
//	printf("Stacks; \n");
//	print_token_stack(eval_stack);
//	print_token_stack(operator_stack);
	// Check if Left Parentheses is actually there (if not tracing from 
	//	 the end.
	if (is_empty(*expr_stack) && !entire_line) { 
		error(tokens[i].t_line, PAREN_MISMATCH);
	} 
	else if (!entire_line) {
		*expr_stack = pop(*expr_stack); // pop left paren
	}
	while (!is_empty(operator_stack)) {
		token op = *(operator_stack->val);
		operator_stack = pop(operator_stack);
		// remove operator
		bool is_unary = false;
		if (op.t_type == NOT || op.t_type == U_STAR || 
				op.t_type == U_MINUS || op.t_type == ACCESS_SIZE) {
			is_unary = true;
		}

		if (is_unary) {
			token b = *(eval_stack->val);
			token answer = eval_uniop(op, b);
			address adr = push_memory(answer);
			eval_stack = pop(eval_stack);
			eval_stack = push(&memory[adr], eval_stack);
		}
		else {
			token a = *(eval_stack->val);
			eval_stack = pop(eval_stack);
			token b = *(eval_stack->val); 
			eval_stack = pop(eval_stack);
			token answer = eval_binop(op, a, b);
			address adr = push_memory(answer);
			eval_stack = push(&memory[adr], eval_stack);
		}
	}
	if (!is_empty(eval_stack)) {
		*expr_stack = push(eval_stack->val, *expr_stack);
	}
	free_stack(eval_stack);
	free_stack(operator_stack);
}

token eval(address start, size_t size) {
//	print_token(&tokens[start]);
//	printf("SIZE: %zd\n", size);
	token_stack* expr_stack = 0;
	//address old_mem = arg_pointer;
	
	if (tokens[start].t_type == AMPERSAND) {
		address mem_to_mod = eval_identifier(start + 1, size - 1);
		token res = make_token(ADDRESS, make_data_num(
				mem_to_mod));
		return res;
	}
	for (address i = start; i <= start + size; i++) {
//		printf("evalling + %d\n", i);
		if (i == start + size) {
			eval_one_expr(i, &expr_stack, LEFT_PAREN, true);
		}
		else if (tokens[i].t_type == RIGHT_PAREN) {
			eval_one_expr(i, &expr_stack, LEFT_PAREN, false);
		}
		else if (tokens[i].t_type == RIGHT_BRACK) {
			eval_one_expr(i, &expr_stack, LEFT_BRACK, true);
		}
		else if (tokens[i].t_type == LEFT_BRACK) {
			// There are two possibilities here. Encountering a left bracket 
			//   could mean either an array reference [], or the start of an 
			//   inline list. If the previous token is empty or an operator,
			//   then it must be an inline list. If it's not an inline list,
			//   we leave the left_brack in the expr because it acts as an 
			//   operator.
			if (is_empty(expr_stack) || precedence(*expr_stack->val)) {
				// It's empty or an operator.
			
				// List! All function calls are lifted out by the preprocessor,
				//   so we only have to scan for square brackets and commas.
				int item_start = i + 1;	
				int item_size = 0;
				int b_count = 0;
				int list_size = 0;
				token* list_elements = malloc(MAX_LIST_INIT_LEN * sizeof(token));
				// i now points to one after the LEFT_BRACKET
				while (true) {
					i++;
					if (i >= start + size) {
						error(tokens[i].t_line, SYNTAX_ERROR, "list");
					}
					if (tokens[i].t_type == RIGHT_BRACK && b_count == 0) {
						if (item_size != 0) {
							token e = eval(item_start, item_size);
							if (e.t_type == RANGE) {
								int start = range_start(e);
								int end = range_end(e);
								for (int i = start; i != end; 
										start < end ? i++ : i--) {
									list_elements[list_size++] = 
										make_token(NUMBER, make_data_num(i));
								}
								list_elements[list_size++] = 
									make_token(NUMBER, make_data_num(end));
							}
							else {
								list_elements[list_size++] = e;
							}
						}
						break;
					}
					else if (tokens[i].t_type == COMMA && b_count == 0) {
						token e = eval(item_start, item_size);
						if (e.t_type == RANGE) {
							int start = range_start(e);
							int end = range_end(e);
							for (int i = start; i != end; 
								start < end ? i++ : i--) {
								list_elements[list_size++] = 
									make_token(NUMBER, make_data_num(i));
							}
							list_elements[list_size++] = 
								make_token(NUMBER, make_data_num(end));
						}
						else {
							list_elements[list_size++] = e;
						}

						item_size = 0;
						item_start = i + 1;
						continue;
					}
					else if (tokens[i].t_type == RIGHT_BRACK) {
						b_count--;
					}
					else if (tokens[i].t_type == LEFT_BRACK) {
						b_count++;
					}
					item_size++;
				}
				// List has now been built.
				address list_h = push_memory_a(list_elements, list_size);
				
				// Create the actual list token.
				token l = make_token(LIST, make_data_num(list_h));
				address a = push_memory(l);

				// Push to stack
				expr_stack = push(&memory[a], expr_stack);
				free(list_elements);
			} // End of inline list 
			else {
				expr_stack = push(&tokens[i], expr_stack);
			}
		}
		else if (tokens[i].t_type == IDENTIFIER) {
			/*if (i < start + size - 1 && 
				tokens[i + 1].t_type == LEFT_BRACK) {
				// An List Reference!
				address id_address = get_address_of_id(tokens[i].t_data.string,
						tokens[i].t_line);
				if (memory[id_address].t_type != LIST) {
					error(tokens[i].t_line, NOT_A_LIST);
				}
				// Pull list header reference from list object.
				id_address = memory[id_address].t_data.number;
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
				// Add 1 to offset because of the header.
				int offset = floor(res.t_data.number) + 1;
				token* a = get_value_of_address(id_address + offset, 
						tokens[eval_start].t_line);
				expr_stack = push(a, expr_stack);
			}
			else {*/
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
			//}
		}
		else if (tokens[i].t_type == TIME) {
			address time_tok = push_memory(time_token());
			expr_stack = push(&memory[time_tok], expr_stack);
		}
		else {
			    // is an operator
			if (precedence(tokens[i]) && 
				// and at the front, or preceded by another operator
				(is_empty(expr_stack) || 
				 (precedence(*(expr_stack->val))) && 
				   expr_stack->val->t_type != ACCESS_SIZE) &&
				(tokens[i].t_type == MINUS || tokens[i].t_type == STAR)) {
				// it's an unary operator!
				tokens[i].t_type = tokens[i].t_type == MINUS ? U_MINUS : U_STAR;
			}
			
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

	// frees the allocated stack
	free_stack(expr_stack);

	// frees the intermediate values in the memory
/*	while (arg_pointer > old_mem) {
		pop_arg();
	}*/
	return result;
}

