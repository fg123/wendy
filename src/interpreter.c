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

void start_program() {
	int brace_count = 0;
	int line_start = 0;
	for (address i = 0; i < tokens_count; i++) {
//		printf("i i %d\n", i);
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
			// Start should just be one behind
			if (line_start != i) {
				error(curr_t.t_line, EXPECTED_END_OF_LINE);
			}
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


void run_tokens(token* _tokens, size_t _length) {
	tokens = _tokens;
	tokens_count = _length;
	start_program();
}

void run(char* input_string) {
	size_t alloc_size = 0;
	tokens_count = scan_tokens(input_string, &tokens, &alloc_size);

	tokens_count = preprocess(&tokens, tokens_count, alloc_size);
	start_program();
}

address eval_identifier(address start, size_t size) {
	// Either we have a *(MEMORY ADDR) or we have identifier[] or identifier
	// I guess we can also have a identifier[][] or any sublists.
	// Also we can have identifier.member
	// We scan until we find right brackets or right parentheses and then
	//   we track back like usual.
//	printf("EVALIDENTIFIER at");
//	print_token(&tokens[start]);
	if (size < 1) {
		error(tokens[start].t_line, INVALID_IDENTIFIER);
		return 0;
	}
	if (tokens[start].t_type == STAR) {
		// Evaluate the Rest and That's the Address
		token t = eval(start + 1, size - 1);
		if (t.t_type != NUMBER && t.t_type != ADDRESS) {
			error(tokens[start].t_line, INVALID_DEREFERENCE);
			return 0;
		}
		else {
			return t.t_data.number;
		}
	}
	if (size == 1) {
		if (tokens[start].t_type != IDENTIFIER) {
			error(tokens[start].t_line, INVALID_IDENTIFIER);
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
			error(tokens[start].t_line, INVALID_LIST_SUBSCRIPT);
			return 0;
		}

		address list_start = arr.t_data.number;
		// add1 because of header
		int offseti = floor(arr_s.t_data.number) + 1;
		
		return list_start + offseti;
	}
	else if (tokens[start].t_type == IDENTIFIER) {
		// Not array reference, but an identifier and also not size 1.
		// AKA we go from the back to find the first DOT
		int j = start + size - 1;
		while (tokens[j].t_type != DOT) {
			j--;
			if (j == start) error(tokens[start].t_line, INVALID_IDENTIFIER);
		}
		token t = eval(start, j - start); 
		if (t.t_type != STRUCT && t.t_type != STRUCT_INSTANCE) {
			error(tokens[start].t_line, NOT_A_STRUCT);
		}
		// Structs can only modify Static members, instances modify instance 
		//   members.
		// Either will be allowed to look through static parameters.
		token e = tokens[start + size - 1];
		int params_passed = 0;
		address metadata = (int)(t.t_data.number);
		if (t.t_type == STRUCT_INSTANCE) {
			// metadata actually points to the STRUCT_INSTANE_HEADER 
			//   right now.
			metadata = (address)(memory[metadata].t_data.number);
		}
		token_type struct_type = t.t_type;
		address struct_header = t.t_data.number;
		while(1) {
			int params_passed = 0;
			address parent_meta = 0;
			int size = (int)(memory[metadata].t_data.number);
			for (int i = 0; i < size; i++) {
				token mdata = memory[metadata + i];
				if (mdata.t_type == STRUCT_STATIC &&
					strcmp(mdata.t_data.string, e.t_data.string) == 0) {
					// Found the static member we were looking for.
					return metadata + i + 1;
				}
				else if (mdata.t_type == STRUCT_PARAM) {
					if (struct_type == STRUCT_INSTANCE && 
						strcmp(mdata.t_data.string, e.t_data.string) == 0) {
						// Found the instance member we were looking for.
						// Address of the STRUCT_INSTANCE_HEADER offset by
						//   params_passed + 1;
						address loc = struct_header + params_passed + 1;
						return loc;
					}
					params_passed++;
				}
				else if (mdata.t_type == STRUCT_PARENT) {
					parent_meta = mdata.t_data.number;
			//		params_passed++;
				}
			}
			// Check now if there is a parent class
			if (parent_meta) {
				metadata = parent_meta;
				struct_header = memory[struct_header + 1].t_data.number;
			}
			else {
				break;
			}
		}

		error(tokens[start].t_line, MEMBER_NOT_EXIST);
	}
	else {
		error(tokens[start].t_line, SYNTAX_ERROR);
	}
	return 0;
}

bool parse_line(address start, size_t size, address* i_ptr) {
	// process the first character
	//printf("Parse Line: Size: %d\n", size);
	//print_token(&tokens[start]);
	if (size <= 0) return false;
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
						make_token(FUNCTION, make_data_num(start + 4))); 
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
					if (i >= start + size) {				
						error(tokens[start].t_line, INCOMPLETE_STATEMENT, "let");
						return false;
					}
				}
				token at_size = eval(start + 3, i - start - 3);
				if (at_size.t_type != NUMBER) {
					error(tokens[start].t_line, INVALID_LIST_SUBSCRIPT);
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
			replace_memory(make_token(FUNCTION, 
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
	else if (f_token.t_type == ASSERT) {
		token result = eval(start + 1, size - 1);
		if (result.t_type != TRUE) {
			error(f_token.t_line, ASSERT_FAIL);	
		}
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
		while(!fgets(buffer, MAX_STRING_LEN, stdin)) {};
					
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
	/*	
		else {
			error(f_token.t_line, INPUT_ERROR);
			return false;
		}*/
	}
	else if (f_token.t_type == REQ) {
		// handled by scanner
		return false;	
	}
	else if (f_token.t_type == STRUCT) {
		// STRUCT SYNTAX
		//   1. STRUCT identifier => (param1, param2 ...);
		//   2. STRUCT identifier => [static1, static2, ...];
		//   3. STRUCT identifier => (param1, ...) [static1, ...];
		//   4. STRUCT identifier:parent => 1, 2, or 3
		if (size < 6) {
			error(f_token.t_line, SYNTAX_ERROR, "struct");
		}
		int sm_size = 0;
		token* struct_meta = malloc(MAX_STRUCT_META_LEN * sizeof(token));
		// Save where the struct metadata will be.
		int i = start + 1;
		if (tokens[i].t_type != IDENTIFIER) {
			error(f_token.t_line, SYNTAX_ERROR, "struct");	
		}
		char* struct_name = tokens[i].t_data.string;
		// Next data gets overwritten at the end with the size;
		struct_meta[sm_size++] = 
			make_token(STRUCT_METADATA, make_data_num(1));
		struct_meta[sm_size++] = make_token(STRUCT_NAME, 
				make_data_str(struct_name));
		
		i++;
		// Check if there is a parent.
		if (tokens[i].t_type == COLON) {
			// Got a parent! Next token should be the identifier of the parent.
			if (tokens[i+1].t_type != IDENTIFIER) {
				error(f_token.t_line, SYNTAX_ERROR, "struct");
			}
			// Check if its a struct.
			token parent = eval(i + 1, 1); 
			if (parent.t_type != STRUCT) {
				error(f_token.t_line, PARENT_NOT_STRUCT);
			}
			// parent's data number is an address to the metadata of the struct.
			// We'll add that to our metadata.
			struct_meta[sm_size++] = make_token(STRUCT_PARENT, 
					parent.t_data);
			struct_meta[sm_size++] = make_token(STRUCT_PARAM,
					make_data_str("base"));
			i += 2; // skip the identifier straight to the deffn
		}
		if (tokens[i].t_type != DEFFN) {
			error(f_token.t_line, SYNTAX_ERROR, "struct");	
		}

		// Default init method.
		struct_meta[sm_size++] = make_token(STRUCT_STATIC, 
						make_data_str("init"));
		struct_meta[sm_size++] = none_token();

		i++;
		// i is now either [, ( or ;
		while (i < start + size) {
			if (tokens[i].t_type == LEFT_BRACK) {
				i++;
				// Handle Static Members
				for (; tokens[i].t_type != RIGHT_BRACK; i ++) {
					if (tokens[i].t_type != IDENTIFIER &&
							tokens[i].t_type != COMMA) {
						error(f_token.t_line, UNEXPECTED_TOKEN, "struct");
						return false;
					}
					else if (tokens[i].t_type == COMMA) {
						continue;
					}
					else {
						struct_meta[sm_size++] = make_token(STRUCT_STATIC, 
							tokens[i].t_data);
						struct_meta[sm_size++] = none_token();
					}
				}
				i++;
			}
			else if (tokens[i].t_type == LEFT_PAREN) {
				// Handle Instance Members
				i++;
				for (; tokens[i].t_type != RIGHT_PAREN; i ++) {
					if (tokens[i].t_type != IDENTIFIER &&
							tokens[i].t_type != COMMA) {
						error(f_token.t_line, UNEXPECTED_TOKEN);
						return false;
					}
					else if (tokens[i].t_type == COMMA) {
						continue;
					}
					else {
						struct_meta[sm_size++] = make_token(STRUCT_PARAM, 
							tokens[i].t_data);
					}
				}
				i++;
			}
			else {
				error(f_token.t_line, SYNTAX_ERROR, "struct");
			}
		}
		struct_meta[0].t_data = make_data_num(sm_size);
		address a = push_memory_array(struct_meta, sm_size);
		// Add Struct to CallStack
//		printf("IN HERE LEL %s\n", struct_name);
		push_stack_entry(struct_name, 
				push_memory(make_token(STRUCT, make_data_num(a))));
		free(struct_meta);
		return false;
	}
	else if (f_token.t_type == COND) {
		if (size < 5) {
			error(f_token.t_line, SYNTAX_ERROR, "if");
		}
		// COND Syntax
		//   1. COND { statement_list; } { statement_list; };
		//   2. COND { s_l; };
		// Run first statement block if top of stack is true, and return at the 
		//   end of the cond chain. Run second statement if false and return
		//   at the next COND chain.
		
		// Get Condition
		token t = pop_arg(f_token.t_line);
		if (t.t_type == TRUE) {
			// Execute first statement list, return pointer at the end.
			push_auto_frame(start + size + 1, "if"); 
			*i_ptr = start + 2;
			return false;
		}
		else if (t.t_type == FALSE) {
			// Run next statement list, if it exists.
			int b = 0;
			int i = start + 2;
			while (true) {
				if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
					break;
				}
				else if (tokens[i].t_type == RIGHT_BRACE) {
					b--;
				}
				else if (tokens[i].t_type == LEFT_BRACE) {
					b++;
				}
				i++;
				if (i >= start + size) {
					error(tokens[i].t_line, INCOMPLETE_STATEMENT, "if");
				}
			}
			// i is now right brace of first
			i++; // now left brace of second
			if (i >= start + size) return false;
			int fn_start = i + 1;
			i++;
			while (true) {
				if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
					break;
				}
				else if (tokens[i].t_type == RIGHT_BRACE) {
					b--;
				}
				else if (tokens[i].t_type == LEFT_BRACE) {
					b++;
				}
				i++;
				if (i >= start + size) {
					error(tokens[i].t_line, INCOMPLETE_STATEMENT, "if");
				}
			}
			// i now right brace of other
			int ra = 0;
			if (tokens[i + 1].t_type == COND) {
				ra = i + 1;
			}
			else {
				ra = start + size + 1;
			}
			push_auto_frame(ra, "evaluating");
			*i_ptr = fn_start;
			return false;
		}
		else {
			error(f_token.t_line, COND_EVAL_NOT_BOOL);
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
		//   1. LOOP { push condition; } LOOP_CONTENT { function block };
	
		int b = 0;
		int i = start + 2;
		while (true) {
			if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
				break;
			}
			else if (tokens[i].t_type == RIGHT_BRACE) {
				b--;
			}
			else if (tokens[i].t_type == LEFT_BRACE) {
				b++;
			}
			i++;
			if (i >= start + size) {
				error(tokens[i].t_line, INCOMPLETE_STATEMENT, "loop");
			}
		}
		// i is now right brace of first
		address ra = i + 1;
		// RA points to LOOP_CONTENT
		tokens[ra].t_data.number = start;	
		push_auto_frame(ra, "evaluating");
		*i_ptr = start + 2;
	}
	else if (f_token.t_type == LOOP_CONTENT) {
		token t = pop_arg(f_token.t_line);
		if (t.t_type != TRUE && t.t_type != FALSE) {
			error(f_token.t_line, COND_EVAL_NOT_BOOL);
			return false;
		}
		if (t.t_type == FALSE) {
			return false;
		}
		int b = 0;	
		int i = start + 1;
		int fn_start = i + 1;
		i++;
		while (true) {
			if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
				break;
			}
			else if (tokens[i].t_type == RIGHT_BRACE) {
				b--;
			}
			else if (tokens[i].t_type == LEFT_BRACE) {
				b++;
			}
			i++;
			if (i >= start + size) {
				error(tokens[i].t_line, INCOMPLETE_STATEMENT, "loop");
			}
		}
		int ra = f_token.t_data.number;
		push_auto_frame(ra, "loop");
		*i_ptr = fn_start;
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
		token t = pop_arg(f_token.t_line);
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
		if (t.t_type == FUNCTION) {
			int ret_val = start + size + 1; // next instruction
			int j = start + size - 1;
			address this_adr = 0;
			while (j > start) {
				if (tokens[j].t_type == DOT) {
//					printf("start is %d j is %d\n", start, j);
//					print_token(&tokens[start]);
					address e = eval_identifier(start + 1, j - start - 1);
					this_adr = e;
					break;					
				}
				j--;
			}

			push_frame(tokens[start + 1].t_data.string, ret_val);
			 
			push_stack_entry(tokens[start + size - 1].t_data.string, a);
			*i_ptr = t.t_data.number;
			if (this_adr) push_stack_entry("this", this_adr);

		}
		else if (t.t_type == STRUCT) {
			// Creating a struct instance! All the arguments are in the stack.
			// We'll take them out in backwards order and assign them to the
			//   instance specific values. First check if there's overloaded 
			//   constructor.
		
			// j stores the address of the start of the metadata
			address j = (int)t.t_data.number; 
			// grab the size of the metadata chain also check if there's an
			//   overloaded init.
			int m_size = memory[j].t_data.number;
			int params = 0;
			address overloaded_init = 0;
			address base_instance_address = 0;	
			for (int i = 0; i < m_size; i++) {
				if (memory[j + i].t_type == STRUCT_PARAM) {
					params++;
				}
				else if (memory[j + i].t_type == STRUCT_PARENT) {
					// Structure has a parent. We'll navigate to the parent
					//   and build an empty struct instance of the parent.
					address p_mdata = (int)memory[j + i].t_data.number;
					int p_size = memory[p_mdata].t_data.number;
					int p_params = 0;
					for (int p = 0; p < p_size; p++) {
						if (memory[p_mdata + p].t_type == STRUCT_PARAM) {
							p_params++;
						}
					}
					int p_total_size = p_params + 1;
					token *parent_instance = malloc(p_total_size * sizeof(token));
					parent_instance[0] = make_token(STRUCT_INSTANCE_HEAD,
							make_data_num(p_mdata));
					for (int p = 1; p < p_total_size; p++) {
						parent_instance[p] = none_token();
					}
					address parent_base_addr = 
						push_memory_array(parent_instance, p_total_size);
					base_instance_address = parent_base_addr;
					free(parent_instance);
				}
				else if (memory[j + i].t_type == STRUCT_STATIC &&
						strcmp("init", memory[j + i].t_data.string) == 0) {
					if (memory[j + i + 1].t_type == NONE) {
						overloaded_init = 0;
					}
					else if (memory[j + i + 1].t_type != FUNCTION) {
						error(t.t_line, INIT_NOT_FN);
					}
					else {
						// Save fn pointer.
						overloaded_init = 
							(address)memory[j + i + 1].t_data.number;	
					}
				}
			}		

			int si_size = 0;
			token* struct_instance = 
				malloc(MAX_STRUCT_META_LEN * sizeof(token));
		
			si_size = params + 1; // + 1 for the header
			struct_instance[0] = make_token(STRUCT_INSTANCE_HEAD, 
					make_data_num(j));
			int offset = params;
			if (base_instance_address) {
				// First param is actually a hidden BASE param
				struct_instance[1] = make_token(STRUCT_INSTANCE,
						make_data_num(base_instance_address));
				offset++;
				si_size++;
			}
			for (int i = 0; i < params; i++) {
				if (overloaded_init) {
					struct_instance[offset - i] = none_token();
				}
				else {
					struct_instance[offset - i] = pop_arg(f_token.t_line);	
				}
			}
			// Struct instance is done.
			address a = push_memory_array(struct_instance, si_size);
			free(struct_instance);
			if (overloaded_init) {
				int ret_val = start + size + 1; // next instruction
//				printf("ret val: %d\n", ret_val);
				address b = push_memory(make_token(STRUCT_INSTANCE, 
							make_data_num(a)));
				push_frame("STRUCT INIT", ret_val); 
				push_stack_entry("this", b);
				*i_ptr = overloaded_init;
			}
			else {
				push_arg(make_token(STRUCT_INSTANCE, make_data_num(a)));
			}
		}
		else {
			error(f_token.t_line, FN_CALL_NOT_FN, 
					tokens[start + size - 1].t_data.string);
		}
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
			error(a.t_line, INVALID_DEREFERENCE);
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
	else if (op.t_type == U_TILDE) {
		// Create copy of object a, only applies to lists or 
		// struct or struct instances
		if (a.t_type == LIST) {
			// We make a copy of the list as pointed to A.
			token list_header = memory[(int)a.t_data.number];
			int list_size = list_header.t_data.number;
			token* new_a = malloc((list_size) * sizeof(token));
			int n = 0;
			address array_start = a.t_data.number;
//			printf("listtsize %d", list_size);
			for (int i = 0; i < list_size; i++) {
				new_a[n++] = memory[array_start + i + 1];
			}
			address new_l_loc = push_memory_a(new_a, list_size);
			free(new_a);
			return make_token(LIST, make_data_num(new_l_loc));
		}
		else if (a.t_type == STRUCT || a.t_type == STRUCT_INSTANCE) {
			// We make a copy of the STRUCT
			address copy_start = a.t_data.number;
			address metadata = (int)(a.t_data.number);
			if (a.t_type == STRUCT_INSTANCE) {
				metadata = memory[metadata].t_data.number;
			}
			int size = memory[metadata].t_data.number + 1;
			if (a.t_type == STRUCT_INSTANCE) {
				// find size of instance by counting instance members
				int actual_size = 0;
				for (int i = 0; i < size; i++) {
					if (memory[metadata + i + 1].t_type == STRUCT_PARAM) {
						actual_size++;
					}
				}
				size = actual_size;
			}
			size++; // for the header itself
			token* copy = malloc(size * sizeof(token));
			for (int i = 0; i < size; i++) {
				copy[i] = memory[copy_start + i];
			}
			address addr = push_memory_array(copy, size);
			free(copy);
			return a.t_type == STRUCT ? make_token(STRUCT, make_data_num(addr))
				: make_token(STRUCT_INSTANCE, make_data_num(addr));
		}
		else {
			// No copy needs to be made
			return a;
		}
	}
	else if (op.t_type == U_MINUS) {
		if (a.t_type != NUMBER) {
			error(a.t_line, INVALID_NEGATE);
			return none_token();
		}
		token res = make_token(NUMBER, make_data_num(-1 * a.t_data.number));
		return res;
	}	
	else if (op.t_type == NOT) {
		if (a.t_type != TRUE &&  a.t_type != FALSE) {
			error(a.t_line, INVALID_NEGATE);
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
		// Array Reference, or String
		// A must be a list/string, b must be a number.
		if (a.t_type != LIST && a.t_type != STRING) {
			error(a.t_line, NOT_A_LIST_OR_STRING);
		}
		token list_header;
		if (a.t_type == LIST) {
			list_header = memory[(int)a.t_data.number];
		}
		int list_size = a.t_type == STRING ? strlen(a.t_data.string) :  
			                                 list_header.t_data.number;
//		printf("LIST SIZE IS: %d\n", list_size);
		// Pull list header reference from list object.
		address id_address = a.t_data.number;
		if (b.t_type != NUMBER && b.t_type != RANGE) {
			error(b.t_line, INVALID_LIST_SUBSCRIPT);
			return none_token();
		}
//		printf("TDATA NUM IS %d\n", (int)(b.t_data.number));
		if ((b.t_type == NUMBER && (int)(b.t_data.number) >= list_size) ||
			(b.t_type == RANGE && 
			((range_start(b) >= list_size || range_end(b) >= list_size ||
			 range_start(b) < 0 || range_end(b) < 0) || 
			range_start(b) == range_end(b)))) {
			error(b.t_line, ARRAY_REF_OUT_RANGE);
		}

		if (b.t_type == NUMBER) {
			if (a.t_type == STRING) {
				token c = make_token(STRING, make_data_str("0"));
				c.t_data.string[0] = a.t_data.string[(int)floor(b.t_data.number)];
				return c;
			}
			// Add 1 to offset because of the header.
			int offset = floor(b.t_data.number) + 1;
			token* c = get_value_of_address(id_address + offset, 
				a.t_line);
			return *c;
		}
		else {
			int start = range_start(b);
			int end = range_end(b);
			int subarray_size = start - end;	
			if (subarray_size < 0) subarray_size *= -1;

			if (a.t_type == STRING) {
				token c = make_token(STRING, make_data_str("0"));
				int n = 0;
				for (int i = start; i != end; start < end ? i++ : i--) {
					c.t_data.string[n++] = a.t_data.string[i];
				}
				c.t_data.string[n] = 0;
				return c;
			}

			subarray_size++;
			
			address array_start = (int)(a.t_data.number);
			token* new_a = malloc(subarray_size * sizeof(token));
			int n = 0;
			for (int i = start; i != end; 
				start < end ? i++ : i--) {
				new_a[n++] = memory[array_start + i + 1];
			}
			new_a[n++] = memory[array_start + end + 1];

			address new_aa = push_memory_a(new_a, subarray_size);
			free(new_a);
			return make_token(LIST, make_data_num(new_aa));
		}
	}
	if (op.t_type == DOT) {
		// Member Access! Supports two built in, size and type.
		// Left side should be a token.
		if (b.t_type != IDENTIFIER) {
			error(b.t_line, MEMBER_NOT_IDEN);
			return false_token();
		}
		if (strcmp("size", b.t_data.string) == 0) {
			return size_of(a);	
		}
		else if (strcmp("type", b.t_data.string) == 0) {
			return type_of(a);
		}
		else {
			// Regular Member, Must be either struct or a struct instance.
			if (a.t_type == STRUCT || a.t_type == STRUCT_INSTANCE) {
				// Either will be allowed to look through static parameters.
				address metadata = (int)(a.t_data.number);
				if (a.t_type == STRUCT_INSTANCE) {
					// metadata actually points to the STRUCT_INSTANE_HEADER 
					//   right now.
					metadata = (address)(memory[metadata].t_data.number);
				}
				token_type struct_type = a.t_type;
				address struct_header = a.t_data.number;
				while(1) {
					int params_passed = 0;
					address parent_meta = 0;
					int size = (int)(memory[metadata].t_data.number);
					for (int i = 0; i < size; i++) {
						token mdata = memory[metadata + i];
						if (mdata.t_type == STRUCT_STATIC &&
							strcmp(mdata.t_data.string, b.t_data.string) == 0) {
							// Found the static member we were looking for.
							return memory[metadata + i + 1];
						}
						else if (mdata.t_type == STRUCT_PARAM) {
							if (struct_type == STRUCT_INSTANCE && 
								strcmp(mdata.t_data.string, b.t_data.string) == 0) {
								// Found the instance member we were looking for.
								// Address of the STRUCT_INSTANCE_HEADER offset by
								//   params_passed + 1;
								address loc = struct_header + params_passed + 1;
								return memory[loc];
							}
							params_passed++;
						}
						else if (mdata.t_type == STRUCT_PARENT) {
							parent_meta = mdata.t_data.number;
						}
					}
					// Check now if there is a parent class
					if (parent_meta) {
						metadata = parent_meta;
						struct_header = memory[struct_header + 1].t_data.number;
					}
					else {
						break;
					}
				}
			}
			error(b.t_line, MEMBER_NOT_EXIST);
			return false_token();
		}
	}
/*	if (op.t_type == TYPEOF) {
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
	}*/
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
				if (has_address) error(a.t_line, TYPE_ERROR, "*");
				return make_token(NUMBER, make_data_num(
									a.t_data.number * b.t_data.number));
			case SLASH:
			case INTSLASH:
			case PERCENT: 
				if (has_address) error(a.t_line, TYPE_ERROR, "%");
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
				error(a.t_line, NUM_NUM_INVALID_OPERATOR);
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
	else if((a.t_type == OBJ_TYPE && b.t_type == OBJ_TYPE) && 
			(op.t_type == EQUAL_EQUAL || op.t_type == NOT_EQUAL)) {
		return (op.t_type == EQUAL_EQUAL) ^ 
			(strcmp(a.t_data.string, b.t_data.string) == 0) ? 
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
				default: error(a.t_line, LIST_LIST_INVALID_OPERATOR); break;
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
			else { error(a.t_line, INVALID_APPEND); }
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
			else { error(a.t_line, INVALID_APPEND); }
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
			error(a.t_line, STRING_NUM_INVALID_OPERATOR);
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

token size_of(token a) {
	double size = 0;
	if (a.t_type == STRING) {
		size = strlen(a.t_data.string);
	}	
	else if (a.t_type == LIST) {
		address h = a.t_data.number;
		size = memory[h].t_data.number;
	}
	return make_token(NUMBER, make_data_num(size));
}

token type_of(token a) {
	switch (a.t_type) {
		case STRING:
			return make_token(OBJ_TYPE, make_data_str("string"));
		case NUMBER:
			return make_token(OBJ_TYPE, make_data_str("number"));
		case TRUE:
		case FALSE:
			return make_token(OBJ_TYPE, make_data_str("bool"));
		case NONE:
			return make_token(OBJ_TYPE, make_data_str("none"));
		case ADDRESS:
			return make_token(OBJ_TYPE, make_data_str("address"));
		case LIST:
			return make_token(OBJ_TYPE, make_data_str("list"));
		case STRUCT:
			return make_token(OBJ_TYPE, make_data_str("struct"));
		case STRUCT_INSTANCE: {
			token instance_loc = memory[(int)(a.t_data.number)]; 
			return make_token(OBJ_TYPE, make_data_str(
				memory[(int)instance_loc.t_data.number + 1].t_data.string));
		}
		default:
			return make_token(OBJ_TYPE, make_data_str("none"));
	}
}

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
		/*case TYPEOF:*/
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
	    case U_TILDE:
		case U_STAR:
			return 160;
		case DOT:
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
					op.t_type == U_MINUS || op.t_type == U_TILDE) {
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
			op.t_type == U_MINUS || op.t_type == U_TILDE) {
			is_unary = true;
		}
		if (is_empty(eval_stack)) {
			error(op.t_line, EVAL_STACK_EMPTY);							
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
						error(tokens[i].t_line, INCOMPLETE_STATEMENT, "list");
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
			if(is_empty(expr_stack) || 
				(expr_stack->val)->t_type != DOT) {
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
			else {
				expr_stack = push(&tokens[i], expr_stack);
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
				 (precedence(*(expr_stack->val)))) &&
				(tokens[i].t_type == MINUS || tokens[i].t_type == STAR 
				 || tokens[i].t_type == TILDE)) {
				// it's an unary operator!
				if (tokens[i].t_type == MINUS) {
					tokens[i].t_type = U_MINUS;
				}
				else if (tokens[i].t_type == STAR) {
					tokens[i].t_type = U_STAR;
				}
				else if (tokens[i].t_type == TILDE) {
					tokens[i].t_type = U_TILDE;
				}
				else {
					error(tokens[i].t_line, EXPECTED_UNARY);
				}
				//tokens[i].t_type = tokens[i].t_type == MINUS ? U_MINUS : U_STAR;
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

