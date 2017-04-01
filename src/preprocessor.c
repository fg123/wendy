#include "preprocessor.h"
#include "token.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scanner.h"

char* p_dump_path = 0;
FILE* p_dump_file = 0;

// Local
static size_t t_curr = 0;
static size_t tokens_alloc_size;
static token* tokens;
static int tmp_count = 1;
static int lambda_count = 0;
static token* new_tokens;
static int length;

// Forward declarations for mutual recursion
void l_process_line(int start, int end);
void l_handle_function(int start, int end, void (*line_fn)(int, int));
int fc_process_fn_call(int start);
void fc_process_line(int start, int end);

void if_process_line(int start, int end);


static void copy_token(token t) {
	new_tokens[t_curr++] = t;
	if (t_curr == tokens_alloc_size) {
		tokens_alloc_size += 200;
		token* tmp = (token*)realloc(new_tokens, tokens_alloc_size * sizeof(token));
		if (tmp) {
			new_tokens = tmp;
		}
		else {
			printf("You have been blessed by a realloc error. Good luck figuring this out!\n");
			exit(1);
		}
	}
}
static void add_token(token_type type, data val) {
	int line = t_curr != 0 ? t_curr - 1 : 0;
	token new_t = { type, line, val };
	new_tokens[t_curr++] = new_t;
	
	if (t_curr == tokens_alloc_size) {
		tokens_alloc_size += 200;
		token* tmp = (token*)realloc(new_tokens, tokens_alloc_size * sizeof(token));
		if (tmp) {
			new_tokens = tmp;
		}
		else {
			printf("You have been blessed by a realloc error. Good luck figuring this out!\n");
			exit(1);
		}
	}

}
 
// printf_tokens(char* appended, char* extension) prints the 
//   tokens to p_dump_file.extension for preprocessor dump.
void printf_tokens(char* appended, char* extension) {
	// i now points to (
	char* full_path = calloc(strlen(p_dump_path) + strlen(extension) + 3, 
			sizeof(char));
	strcpy(full_path, p_dump_path);
	strcat(full_path, ".");
	strcat(full_path, extension);
	p_dump_file = fopen(full_path, "w");

	if (p_dump_file) {
		fprintf(p_dump_file, "WendyScript Pre-Processor Dump: %s\n\n", appended);
		int indent = 0;
		fprintf(p_dump_file,"%8d: ", 1);
		print_token_inline(&tokens[0], p_dump_file);
		fprintf(p_dump_file, " ");
		for (int i = 1; i < length; i++) {
			if(tokens[i].t_type == RIGHT_BRACE) indent--;

			if(tokens[i - 1].t_type == SEMICOLON || 
				tokens[i - 1].t_type == LEFT_BRACE || 
				(tokens[i - 1].t_type == RIGHT_BRACE  &&
				 (tokens[i].t_type != SEMICOLON && tokens[i].t_type != COMMA) )) {
				fprintf(p_dump_file,"\n%8d: ", i + 1);
				for(int j = 0; j < indent; j++) fprintf(p_dump_file, "  ");
			}
		
			print_token_inline(&tokens[i], p_dump_file);
		

			if ((i != length - 1 && tokens[i + 1].t_type != SEMICOLON &&
						tokens[i + 1].t_type != RIGHT_PAREN &&
						tokens[i + 1].t_type != COMMA) &&
				tokens[i].t_type != LEFT_PAREN) {
				fprintf(p_dump_file, " ");
			}
			if(tokens[i].t_type == LEFT_BRACE) indent++;

		}
		fclose(p_dump_file);
	}

}

// make_lambda(start, end) actually lifts out the lambda function into a new
//   function
// requires: start points to (, end points to }
void make_lambda(int start, int end) {
	// run a check on the entire lambda
	int old_count = lambda_count;
	// run check on the contents of the lambda function
	// now we map it out
	add_token(LET, make_data_str("let"));
	add_token(IDENTIFIER, make_data_str(""));
	snprintf(new_tokens[t_curr - 1].t_data.string, MAX_STRING_LEN, 
			"$lambda%d", lambda_count++);
	add_token(DEFFN, make_data_str("=>"));
	int i = start;
	// we find the left brace
	while (tokens[i].t_type != LEFT_BRACE) {
		copy_token(tokens[i]);
		i++; 
		if (i >= length) { 
			error(tokens[i].t_line, SYNTAX_ERROR);
		}
	}
	add_token(LEFT_BRACE, make_data_str("{"));
	l_handle_function(i + 1, end, l_process_line);
	add_token(RIGHT_BRACE, make_data_str("}"));
	add_token(SEMICOLON, make_data_str(";"));
}

// if_process_line(start, end) takes a line of code and checks for if statements
//   and appropriately converts them to bytecode
//
//   Given:
//   if (condition1) { statement1; } 
//   else if (condition2) { statement2; } 
//   else { statement3; }; 
//
//   Outputs:
//   push condition1;
//   cond { statement1; } { push condition2; } 
//   cond { statement2; } { statement3 };
//
//   Cond chain is one line, this allows short circuit evaluation with the
//     function call bytecode.
void if_process_line(int start, int end) {
	if (tokens[start].t_type == IF) {
		// The first condition get's pushed on the stack.
		int i = start + 1;
		if (tokens[i].t_type != LEFT_PAREN) {
			error(tokens[i].t_line, SYNTAX_ERROR);
		}
		i++; // i now points to the beginning of the first condition
		add_token(PUSH, make_data_str("push"));
		int b = 0;
		while (true) {
			if (tokens[i].t_type == RIGHT_PAREN && b == 0) {
				break;
			}
			else if (tokens[i].t_type == RIGHT_PAREN) {
				b--;
			}
			else if (tokens[i].t_type == LEFT_PAREN) {
				b++;
			}
			copy_token(tokens[i]);
			i++;
			if (i >= end) {
				error(tokens[i].t_line, SYNTAX_ERROR);
			}
		}
		// i now points to the right paren, now we begin the cond chain
		add_token(SEMICOLON, make_data_str(";"));
		bool continue_chain = true;
		// First condition is pushed on the stack.
		do {
			// This loop requires that i + 1 is the left brace of the statement.
			add_token(COND, make_data_str("cond"));
			if (tokens[i + 1].t_type != LEFT_BRACE) {
				error(tokens[i].t_line, SYNTAX_ERROR);
			}
			i++; // i now is at the left brace
			copy_token(tokens[i]); // copy left brace
			int s_start = i + 1;
			int b = 0;
			while (1) {
				i++;
				if (i >= end) {
					//printf("FUCK %d\n", i);
					error(tokens[i].t_line, SYNTAX_ERROR);
				}
				else if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
				//	printf("Found End\n");
					break;
				}
				else if (tokens[i].t_type == LEFT_BRACE) {
					b++;
				}
				else if (tokens[i].t_type == RIGHT_BRACE) {
					b--;
				}
			}
			// Do the inner substatementlist.
			l_handle_function(s_start, i, if_process_line);
			// i now at right brace
			if (tokens[i].t_type != RIGHT_BRACE) {
				error(tokens[i].t_line, SYNTAX_ERROR);
			}
			copy_token(tokens[i]); // copy right brace

			// Check next token, should either be end, ELSEIF or ELSE
			i++;
			if (i >= end) {
				// end
				continue_chain = false;
			}
			else if (tokens[i].t_type == ELSEIF) {
				i++;
				if (tokens[i].t_type != LEFT_PAREN) {
					error(tokens[i].t_line, SYNTAX_ERROR);
				}
				i++; // i now points to the beginning of the first condition
				add_token(LEFT_BRACE, make_data_str("{"));
				add_token(PUSH, make_data_str("push"));
				int b = 0;
				while (true) {
					if (tokens[i].t_type == RIGHT_PAREN && b == 0) {
						break;
					}
					else if (tokens[i].t_type == RIGHT_PAREN) {
						b--;
					}
					else if (tokens[i].t_type == LEFT_PAREN) {
						b++;
					}
					copy_token(tokens[i]);
					i++;
					if (i >= end) {
						error(tokens[i].t_line, SYNTAX_ERROR);
					}
				}
				// i now at right paren
				add_token(SEMICOLON, make_data_str(";"));
				add_token(RIGHT_BRACE, make_data_str("}"));

				// continue chain
			}
			else if (tokens[i].t_type == ELSE) {
				i++;
				if (tokens[i].t_type != LEFT_BRACE) {
					error(tokens[i].t_line, SYNTAX_ERROR);
				}
				copy_token(tokens[i]); // copy left brace
				// Do the inner substatementlist, end of the whole thing is
				//  the end of the else hopefully.
				l_handle_function(i + 1, end, if_process_line);
				if (tokens[end - 1].t_type != RIGHT_BRACE) {
					error(tokens[end].t_line, SYNTAX_ERROR);
				}
				copy_token(tokens[end - 1]); // copy right brace
				continue_chain = false;
			}
			else {
				error(tokens[i].t_line, SYNTAX_ERROR);
			}
		} while (continue_chain);
		// End of cond chain
		add_token(SEMICOLON, make_data_str(";"));

	}
	else {
		// Not an if statement, copy token line as usual!
		for (int i = start; i < end; i++) {
			if (tokens[i].t_type == LEFT_BRACE) {
				// find end of fn
				copy_token(tokens[i]);
				int s_start = i + 1;
				int b = 0;
				while (1) {
					i++;
					if (i >= end) {
						//printf("FUCK %d\n", i);
						error(tokens[i].t_line, SYNTAX_ERROR);
					}
					else if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
					//	printf("Found End\n");
						break;
					}
					else if (tokens[i].t_type == LEFT_BRACE) {
						b++;
					}
					else if (tokens[i].t_type == RIGHT_BRACE) {
						b--;
					}
				}
				l_handle_function(s_start, i, if_process_line);
				copy_token(tokens[i]);
			}
			else {
				copy_token(tokens[i]);
			}
		}
		add_token(SEMICOLON, make_data_str(";"));
	}
}

// l_process_line(start, end) takes a line of code (like parse_line)
//   and checks for lambdas
void l_process_line(int start, int end) {
	// first we scan to see if there are embedded lambda functions
	int old_count = lambda_count;
	for (int i = start; i < end; i++) {
		if (tokens[i].t_type == LAMBDA) {
			// Read until the end of the embedded lambda and lift
			int f_start = i + 1;
			int f_end = 0;
			while (tokens[i].t_type != LEFT_BRACE) { 
				i++; 
				if (i >= length) { 
					error(tokens[i].t_line, SYNTAX_ERROR);
				}
			}
			int brace_count = 0;
			while (1) {
				i++;
				if (i >= length) {
					error(tokens[i].t_line, SYNTAX_ERROR);
				}
				if (tokens[i].t_type == RIGHT_BRACE && brace_count == 0) {
					f_end = i;
					break;
				}
				else if (tokens[i].t_type == LEFT_BRACE) {
					brace_count++;
				}
				else if ( tokens[i].t_type == RIGHT_BRACE) {
					brace_count--;
				}
			}
			// Now we lift out the embedded lambda
			make_lambda(f_start, f_end); 
			tokens[f_start].t_data.number = f_end;
		}
	}
	for (int i = start; i < end; i++) {
		if (tokens[i].t_type == LAMBDA) {
			// already lifted, so we replace with old_count
			add_token(IDENTIFIER, make_data_str(""));
			snprintf(new_tokens[t_curr - 1].t_data.string, MAX_STRING_LEN, 
				"$lambda%d", old_count++);
			i = tokens[i + 1].t_data.number;
		}
		else {
			// just copy like normal
//			printf("Copied: ");
			//print_token(&tokens[i]);
			copy_token(tokens[i]);
		}
	}
	add_token(SEMICOLON, make_data_str(";"));
}

// fc_eval_one evaluates one argument of a functino call to see if there are
//   more
void fc_eval_one(int start, int end) {
	for (int i = start; i < end - 1; i++) {
		if (tokens[i].t_type == IDENTIFIER && 
			tokens[i + 1].t_type == LEFT_PAREN &&
			tokens[i].t_data.string[0] != '~') {
			// Function Call within the Line
			i = fc_process_fn_call(i);
		}
	}
}

// require start points to the identifier and returns the end of the fn_call
int fc_process_fn_call(int start) {
	int i = start + 2; 
	// first we pull the arguments by reading until the matching )
	//   in the function call
	int arg_vals_start[100];
	int arg_vals_end[100];

	int arg_start = i;
	int arg_eval_size = 0;
	int argc = 0;
	int brace_count = 0;
	int old_count = tmp_count;
	while (1) {
		if (tokens[i].t_type == RIGHT_PAREN && arg_eval_size == 0) {
			// no arguments
			break;
		}
		else if ((tokens[i].t_type == RIGHT_PAREN || 
				 tokens[i].t_type == COMMA) && brace_count == 0) {
			// has an argument, throw that to be processed too
//			printf("FC_EVALONEARG %d, %d\n", arg_start, arg_start + arg_eval_size);
			fc_eval_one(arg_start, arg_start + arg_eval_size);
//			printf("Done FC\n");
			arg_vals_start[argc] = arg_start;
			arg_vals_end[argc] = arg_start + arg_eval_size;
			argc++;
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
		if (i >= length) {
			error(tokens[i].t_line, SYNTAX_ERROR);
		}
//		printf("%d\n", i);
	}
///	printf("Building PUSH arg\n");
	for (int i = 0; i < argc; i++) {
		// push each argument
		add_token(PUSH, make_data_str("push"));
		for (int j = arg_vals_start[i]; j < arg_vals_end[i]; j++) {
			if (tokens[j].t_type == IDENTIFIER && 
					tokens[j].t_data.string[0] == '~') {
				copy_token(tokens[j]);
//				printf("%d\n", j);
				j = tokens[j + 1].t_data.number;
			}
			else {
				copy_token(tokens[j]);
			}
		}
		add_token(SEMICOLON, make_data_str(";"));
	}
	add_token(CALL, make_data_str("call"));
	copy_token(tokens[start]);
	add_token(SEMICOLON, make_data_str(";"));

	add_token(LET, make_data_str("let"));
	add_token(IDENTIFIER, make_data_str(""));
	snprintf(new_tokens[t_curr - 1].t_data.string, MAX_STRING_LEN, 
			"~tmp%d", tmp_count);
	add_token(SEMICOLON, make_data_str(";"));
	add_token(POP, make_data_str("pop"));
	add_token(IDENTIFIER, make_data_str(""));
	snprintf(new_tokens[t_curr - 1].t_data.string, MAX_STRING_LEN, 
			"~tmp%d", tmp_count);
	add_token(SEMICOLON, make_data_str(";"));
	snprintf(tokens[start].t_data.string, MAX_STRING_LEN, 
			"~tmp%d", tmp_count);
	tokens[start + 1].t_data.number = i;
	tmp_count++;

	return i;
}

// fc_process_line, for functino calls
void fc_process_line(int start, int end) {
//	printf("Processing Line %d - %d\n", start, end);
	if (tokens[start].t_type == STRUCT || tokens[start].t_type == REQ) {
		return;
	}
	int ends[100] = {0};
	int emb_fn_count = 0;
	for (int i = start; i < end - 1; i++) {
//		printf("%d\n", i);
		if (tokens[i].t_type == LEFT_BRACE) {
			// find end of fn
			int start = i + 1;
			int b = 0;
			while (1) {
				i++;
				if (i >= end) {
					//printf("FUCK %d\n", i);
					error(tokens[i].t_line, SYNTAX_ERROR);
				}
				else if (tokens[i].t_type == RIGHT_BRACE && b == 0) {
				//	printf("Found End\n");
					break;
				}
				else if (tokens[i].t_type == LEFT_BRACE) {
					b++;
				}
				else if (tokens[i].t_type == RIGHT_BRACE) {
					b--;
				}
			}	
			ends[emb_fn_count++] = i;
		}
		else if (tokens[i].t_type == IDENTIFIER && 
			tokens[i + 1].t_type == LEFT_PAREN &&
			tokens[i].t_data.string[0] != '~') {
			// Function Call within the Line
			int end = fc_process_fn_call(i);
			tokens[i + 1].t_data.number = end;
		}
	}
	int c = 0;
	for (int i = start; i < end; i++) {
//		printf("%d\n", i);
		if (tokens[i].t_type == LEFT_BRACE) {
			copy_token(tokens[i]);
			l_handle_function(i + 1, ends[c], fc_process_line);
			i = ends[c++];
		}
		else if (tokens[i].t_type == IDENTIFIER && 
					tokens[i].t_data.string[0] == '~') {
			copy_token(tokens[i]);
			i = tokens[i + 1].t_data.number;
		}
		else {
			copy_token(tokens[i]);
		}
	}
	add_token(SEMICOLON, make_data_str(";"));
}

// applies line_fn to each line
void l_handle_function(int start, int end, void (*line_fn)(int, int)) {
	int brace_count = 0;
	int line_start = start;
//	printf("start:  %d, %d\n", start, end);
	for (int i = start; i < end; i++) {
		token curr_t = tokens[i];
//
//		if(line_fn == fc_process_line) printf("brace_Count: %d\n", brace_count);		
		if (curr_t.t_type == SEMICOLON && brace_count == 0) {
//			printf("Process: ");
//			print_token(&tokens[line_start]);
			line_fn(line_start, i);
			line_start = i + 1;
		}
		else if (i == end && curr_t.t_type != SEMICOLON) {
			// invalid end of line
			error(curr_t.t_line, EXPECTED_END_OF_LINE, tokens[i].t_data.string);
		}
		else if (curr_t.t_type == LEFT_BRACE) {
			brace_count++;
		}
		else if (curr_t.t_type == RIGHT_BRACE) {
			brace_count--;
		}
//		printf("%d\n", i);
	}
}

// reset_counters used to reset when making another pass
void reset_counters() {
	// next round
	free(tokens);
	tokens = new_tokens;
	new_tokens = malloc(tokens_alloc_size * sizeof(token));
	length = t_curr;
	t_curr = 0;
}

size_t preprocess(token** _tokens, size_t _length, size_t _alloc_size) {
	// Setup
	tokens = *_tokens;
	tokens_alloc_size = _alloc_size;
	new_tokens = malloc(tokens_alloc_size * sizeof(token));
	length = _length;
	
	if (p_dump_path) printf_tokens("Scanned Tokens", "scanned");

	// Lift out lambda functions: 	
	l_handle_function(0, _length, l_process_line);

	reset_counters();
	if (p_dump_path) printf_tokens("Lambdas Lift-out", "lambda");
	
	// Process conditionals
	l_handle_function(0, length, if_process_line);
	reset_counters();
	if (p_dump_path) printf_tokens("Conditionals", "cond");

	// Second pass, we scan for a lambda symbol. 
	// What are we looking for? We look for function definition headers, eg
	//   a =>, following which is the argument list, and also 
	//   looking for function calls.
	for (int i = 0; i < length; i++) {
		if (tokens[i].t_type == STRUCT ||
				tokens[i].t_type == REQ) {
			// ignore until the end, already processed
			while (i < length && tokens[i].t_type != SEMICOLON) i++;
			
		}
		else if (tokens[i].t_type == B_COMMENT_START) {
			// Scan until end of block comment
			while (i < length && tokens[i].t_type != B_COMMENT_END) i++;
		}
		else if (tokens[i].t_type == DEFFN) {
			// Found a function header
			int fn_start = i + 1;
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
				}
			}
			// fn_start is now at the right_paren 
			//	 (next iteration will increment)
			i = fn_start + 1; 
			add_token(DEFFN, make_data_str("=>"));
			add_token(LEFT_BRACE, make_data_str("{"));
			// set up pop instructions for the arguments
			for (int j = req_arg_count - 1; j >= 0; j--) {
				add_token(LET, make_data_str("let"));
				add_token(IDENTIFIER, make_data_str(""));
				strcpy(new_tokens[t_curr - 1].t_data.string, arg_ids[j]);
				add_token(SEMICOLON, make_data_str(";"));
				add_token(POP, make_data_str("pop"));
				add_token(IDENTIFIER, make_data_str(""));
				strcpy(new_tokens[t_curr - 1].t_data.string, arg_ids[j]);
				add_token(SEMICOLON, make_data_str(";"));
			}
		}
		else {
			copy_token(tokens[i]);
		}
	}
	reset_counters();
	//print_token_list(tokens, length);	
	if (p_dump_path) printf_tokens("Function Definitions", "fndef");


	// Third Pass: we convert all function calls, very similar to lambdas
	l_handle_function(0, length, fc_process_line);
	reset_counters();
	if (p_dump_path) printf_tokens("Function Calls", "fncall");

//	print_token_list(new_tokens, t_curr);
	

	// Free Old Block
//	free(tokens);
	*_tokens = tokens;
	return length;
}


