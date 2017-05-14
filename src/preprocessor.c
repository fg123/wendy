#include "preprocessor.h"
#include "token.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
#include <assert.h>

char* p_dump_path = 0;
FILE* p_dump_file = 0;
char* compile_path = 0;
FILE* compile_file = 0;

// Local
static size_t t_curr = 0;
static size_t tokens_alloc_size;
static token* tokens;
static int tmp_count = 1;
static int lambda_count = 0;
static token* new_tokens;
static int length;
static int cur_line = 0;

// Forward declarations for mutual recursion
void l_process_line(int start, int end);
void l_handle_function(int start, int end, void (*line_fn)(int, int));
int fc_process_fn_call(int start, int end);
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
			w_error(REALLOC_ERROR);
		}
	}
}
static void add_token(token_type type, data val) {
//	int line = t_curr != 0 ? t_curr - 1 : 0;
	token new_t = { type, cur_line, val };
	new_tokens[t_curr++] = new_t;
	
	if (t_curr == tokens_alloc_size) {
		tokens_alloc_size += 200;
		token* tmp = (token*)realloc(new_tokens, tokens_alloc_size * sizeof(token));
		if (tmp) {
			new_tokens = tmp;
		}
		else {
			w_error(REALLOC_ERROR);
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
	cur_line = tokens[start].t_line;
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
			error(tokens[i].t_line, INCOMPLETE_LAMBDA);
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
	cur_line = tokens[start].t_line;

	if (tokens[start].t_type == LOOP) {
		copy_token(tokens[start]);
		// The condition gets wrapped and put on the stack
		int i = start + 1;
		if (tokens[i].t_type != LEFT_PAREN) {
			error(tokens[i].t_line, SYNTAX_ERROR, "loop");
		}
		add_token(LEFT_BRACE, make_data_str("{"));
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
				error(tokens[i].t_line, INCOMPLETE_IF);
			}
		}
		// i now points to the right paren, now we begin the cond chain
		add_token(SEMICOLON, make_data_str(";"));
		add_token(RIGHT_BRACE, make_data_str("}"));
		add_token(LOOP_CONTENT, make_data_str("LOOP=>"));	
		i++; // i now is at the left brace
		copy_token(tokens[i]); // copy left brace
		int s_start = i + 1;
		b = 0;
		while (1) {
			i++;
			if (i >= end) {
				//printf("FUCK %d\n", i);
				error(tokens[i].t_line, INCOMPLETE_IF);
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
			error(tokens[i].t_line, SYNTAX_ERROR, "loop");
		}
		copy_token(tokens[i]); // copy right brace
		add_token(SEMICOLON, make_data_str(";"));
	}
	else if (tokens[start].t_type == IF) {
		// The first condition get's pushed on the stack.
		int i = start + 1;
		if (tokens[i].t_type != LEFT_PAREN) {
			error(tokens[i].t_line, SYNTAX_ERROR, "if");
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
				error(tokens[i].t_line, INCOMPLETE_IF);
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
				error(tokens[i].t_line, INCOMPLETE_IF);
			}
			i++; // i now is at the left brace
			copy_token(tokens[i]); // copy left brace
			int s_start = i + 1;
			int b = 0;
			while (1) {
				i++;
				if (i >= end) {
					//printf("FUCK %d\n", i);
					error(tokens[i].t_line, INCOMPLETE_IF);
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
				error(tokens[i].t_line, SYNTAX_ERROR, "if");
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
					error(tokens[i].t_line, SYNTAX_ERROR, "if");
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
						error(tokens[i].t_line, INCOMPLETE_IF);
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
					error(tokens[i].t_line, SYNTAX_ERROR, "if");
				}
				copy_token(tokens[i]); // copy left brace
				// Do the inner substatementlist, end of the whole thing is
				//  the end of the else hopefully.
				l_handle_function(i + 1, end, if_process_line);
				if (tokens[end - 1].t_type != RIGHT_BRACE) {
					error(tokens[end].t_line, SYNTAX_ERROR, "if");
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
						error(tokens[i].t_line, INCOMPLETE_STATEMENT_LIST);
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
					error(tokens[i].t_line, INCOMPLETE_LAMBDA);
				}
			}
			int brace_count = 0;
			while (1) {
				i++;
				if (i >= length) {
					error(tokens[i].t_line, INCOMPLETE_LAMBDA);
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
void fc_eval_one(int start, int end, bool fn_argument) {
	cur_line = tokens[start].t_line;

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
					error(tokens[i].t_line, INCOMPLETE_FN_CALL);
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
		else if (tokens[i].t_type == LEFT_PAREN) {
			// POTENTIAL FUNCTION CALL
			// Scan previous tokens to find the caller maybe..
			int l_paren_loc = i;
			i--;
			while (tokens[i].t_type == IDENTIFIER || tokens[i].t_type == DOT) {
				// As long as they are identifiers or dots, we traverse back.
				i--;
				if (i < start) break;
			}
			i++; // to go back to the identifier or the dot
			if (i < start || i == l_paren_loc) {
				// i is out or bounds or hasn't changed
				i = l_paren_loc; // Reset then go back to outer loop.
				continue;
			}
			if (tokens[i].t_type != IDENTIFIER) {
				// Function call did not start with an identifier...
				error(tokens[i].t_line, INCOMPLETE_FN_CALL);
			}
			int tend = fc_process_fn_call(i, end);
			// tend now is the end of that function call, the right parentheses.
			for (int j = i; j < tend; j++) {
				// Replace with emptys.
				tokens[j].t_type = EMPTY;
			}
			i = tend;
		}
/*		else if (tokens[i].t_type == IDENTIFIER) {
			// Find until no period
			int start = i;
			while (tokens[i].t_type == IDENTIFIER &&
					tokens[i + 1].t_type == DOT) {
				i += 2;
			}
			if (tokens[i + 1].t_type == LEFT_PAREN  &&
				tokens[start].t_data.string[0] != '~') {
				// Function Call within the Line
				int tend = fc_process_fn_call(start, end);
				tokens[i + 1].t_data.number = tend;
				i = tend;
			}
		}*/
	}
	int c = 0;
	for (int i = start; i < end; i++) {
	//	printf("%d\n", i);
		if (tokens[i].t_type == LEFT_BRACE) {
			copy_token(tokens[i]);
			l_handle_function(i + 1, ends[c], fc_process_line);
			i = ends[c++];
			copy_token(tokens[i]);
		}
		else if (tokens[i].t_type == EMPTY) {
			// Do nothing
		}
		else if (!fn_argument) {
			copy_token(tokens[i]);
		}
	}
	if (!fn_argument) {
		add_token(SEMICOLON, make_data_str(";"));
	}
}

// require start points to the identifier and returns the end of the fn_call
int fc_process_fn_call(int start, int end) {
//	printf("in here with %d to %d\n", start, end);
	cur_line = tokens[start].t_line;

	int i = start;
	while (tokens[i].t_type != LEFT_PAREN) {
		i++;
		if (i >= end) return -1;
	}
	int id_end = i++;
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
			fc_eval_one(arg_start, arg_start + arg_eval_size, true);
//			printf("Done FC\n");
			arg_vals_start[argc] = arg_start;
			arg_vals_end[argc] = arg_start + arg_eval_size;
			argc++;
			arg_start = i + 1;
			arg_eval_size = 0;
			if (tokens[i].t_type == RIGHT_PAREN) { break; }
			i++;
		}
		else if (tokens[i].t_type == RIGHT_PAREN || tokens[i].t_type == RIGHT_BRACK) {
			brace_count--;
			arg_eval_size++;
			i++;
		}
		else if (tokens[i].t_type == LEFT_PAREN || tokens[i].t_type == LEFT_BRACK) {
			brace_count++;
			arg_eval_size++;
			i++;
		}
		else {
			arg_eval_size++;
			i++;
		}
		if (i >= length) {
			error(tokens[i].t_line, INCOMPLETE_FN_CALL);
		}
//		printf("%d\n", i);
	}
	int endcall = i;
///	printf("Building PUSH arg\n");
	for (int i = 0; i < argc; i++) {
		// push each argument
		add_token(PUSH, make_data_str("push"));
		for (int j = arg_vals_start[i]; j < arg_vals_end[i]; j++) {
			if (tokens[j].t_type != EMPTY) {
				copy_token(tokens[j]);
			}
		}
		add_token(SEMICOLON, make_data_str(";"));
	}
	add_token(CALL, make_data_str("call"));
	for (int i = start; i < id_end; i++) {
		copy_token(tokens[i]);
	}
	add_token(SEMICOLON, make_data_str(";"));

	/*add_token(LET, make_data_str("let"));
	add_token(IDENTIFIER, make_data_str(""));
	snprintf(new_tokens[t_curr - 1].t_data.string, MAX_STRING_LEN, 
			"~tmp%d", tmp_count);
	add_token(SEMICOLON, make_data_str(";"));*/
	add_token(POP, make_data_str("pop"));
	add_token(IDENTIFIER, make_data_str(""));
	snprintf(new_tokens[t_curr - 1].t_data.string, MAX_STRING_LEN, 
			"~tmp%d", tmp_count);
	int newidtoken = t_curr - 1;

	tmp_count++;

	add_token(SEMICOLON, make_data_str(";"));
	tokens[endcall] = new_tokens[newidtoken];
	return endcall;
}

// fc_process_line, for functino calls
void fc_process_line(int start, int end) {
//	printf("Processing Line %d - %d\n", start, end);
	if (tokens[start].t_type == REQ) {
		return;
	}
	// False because not a fn argument
	fc_eval_one(start, end, false);
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
		cur_line = tokens[i].t_line;

		if (tokens[i].t_type == REQ) {
			// ignore until the end, already processed
			while (i < length && tokens[i].t_type != SEMICOLON) i++;
			
		}
		else if (tokens[i].t_type == STRUCT) {
			while (i < length && tokens[i].t_type != SEMICOLON) {
				copy_token(tokens[i]);
				i++;
			}
			copy_token(tokens[i]);
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
				/*add_token(LET, make_data_str("let"));
				add_token(IDENTIFIER, make_data_str(""));
				strcpy(new_tokens[t_curr - 1].t_data.string, arg_ids[j]);
				add_token(SEMICOLON, make_data_str(";"));*/
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
	if (compile_path) {
		FILE* compiled = fopen(compile_path, "w");
		if (compiled) {
			tokens_to_file(tokens, length, compiled);
		}

		fclose(compiled);
		return 0;
	}
	*_tokens = tokens;
	return length;
}

token* file_to_tokens(FILE* file, size_t* size) {
	assert(file);
	// We read the bytes from the file and process it into a
	//   list of tokens.
	int total_size;
	char header[40];
	fscanf(file, "%s", header);
	if (strcmp("WendyObj", header) != 0) {
		w_error(INVALID_P_FILE);		
	}
	fscanf(file, "%d", &total_size);
	fgetc(file);
	token* tokens = malloc(total_size * sizeof(tokens));
	int i = 0;
	while (i < total_size) {
		token next;
		next.t_line = 0;
		char type;
		fscanf(file, "%c", &type);

		next.t_type = (token_type)type;
		next.t_data.string[0] = 0;
		if (next.t_type == NUMBER) {
			// Number Literal, read the double
			fread(&(next.t_data.number), sizeof(double), 1, file);
		}
		else if (next.t_type == STRING || next.t_type == IDENTIFIER) {
			char item;
			int n = 0;
			do {
				fscanf(file, "%c", &item);
				next.t_data.string[n++] = item;
			} while (item);
		} 
		tokens[i++] = next;

	}
	*size = i;
	return tokens;
}

void tokens_to_file(token* tokens, size_t length, FILE* file) {
	fprintf(file, "WendyObj %zd\n", length);
	for(int i = 0; i < length; i++) {
		fprintf(file, "%c", tokens[i].t_type);
		if (tokens[i].t_type == NUMBER) {
			fwrite(&(tokens[i].t_data.number), sizeof(double), 1, file);
		}
		else if (tokens[i].t_type == STRING || tokens[i].t_type == IDENTIFIER) {
			int length = strlen(tokens[i].t_data.string);
			fprintf(file, "%s", tokens[i].t_data.string);
			fprintf(file, "%c", 0);
		}
	}
}
