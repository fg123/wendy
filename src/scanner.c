#include "scanner.h"
#include <stdbool.h>
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <execpath.h>
#include "global.h"

static size_t source_len;
static size_t current; // is used to keep track of source current
static size_t start;
static char* source;
static token* tokens;
static size_t tokens_alloc_size;
static size_t t_curr; // t_curr is used to keep track of addToken
static size_t line;
static size_t col;

static void add_token(token_type type);
static void add_token_V(token_type type, data val);

// is_at_end() returns true if scanning index reached the end
static bool is_at_end() {
	return current >= source_len;
}

// match(expected, source) advances the index and checks the next character
static bool match(char expected) {
	if (is_at_end()) return false;
	if (source[current] != expected) return false;
	col++;
	current++;
	return true;
}

// peek(char) returns the next character or \0 if we are at the end
static char peek() {
	if (is_at_end()) return '\0';
	return source[current];
}

// advance() returns the current character then moves onto the next one
static char advance() {
	current++;
	col++;
	return source[current - 1];
}
// peek_next() peeks at the character after the next or \0 if at end
static char peek_next() {
	if(current + 1 >= source_len) return '\0';
	return source[current + 1];
}

// is_alpha(c) returns true if c is alphabet
static bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
}

// is_digit(c) returns true if c is digit
static bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

// is_alpha_numeric(c) returns true if c is alpha AND numeric
static bool is_alpha_numeric(char c) {
	return is_alpha(c) || is_digit(c);
}

// handle_import() imports the string contents of the given file, otherwise
//   leaves it for code_gen
static void handle_import() {
	
	add_token(REQ);	
	// we scan a string
	// t_curr points to space after REQ
	start = current;
	while(!scan_token()) {
		start = current;
	}
	
	if (tokens[t_curr - 1].t_type == STRING) {
		int str_loc = t_curr - 1;
		char* path = tokens[str_loc].t_data.string;
		long length = 0;
		
		char* buffer;
		FILE * f = fopen(path, "r");
		//printf("Attempting to open %s\n", path);
		if (f) {
			fseek (f, 0, SEEK_END);
			length = ftell (f);
			fseek (f, 0, SEEK_SET);
			// + 2 for null terminator and newline
			buffer = safe_malloc(length + 2);
			buffer[0] = '\n';
			if (buffer) {
				fread(buffer + 1, 1, length, f);
				buffer[length] = '\0'; // fread doesn't add a null terminator!
				int newlen = source_len + strlen(buffer);
				source = safe_realloc(source, newlen + 1);
				size_t o_source_len = source_len;
				source_len = newlen;
				// at this point, we inset the buffer into the source
				// current should be the next item, we move it down buffer bytes
				//printf("old source: \n%s\n", source);
				memmove(&source[current + strlen(buffer)], 
						&source[current], o_source_len - current + 1);
				memcpy(&source[current], buffer, strlen(buffer));
				//printf("newsource: \n%s\n", source);
			}
			safe_free(buffer);
			fclose (f);
		}
		else {
			error_lexer(line, col, SCAN_REQ_FILE_READ_ERR);
		}
		//safe_free(path);
	}
}

// handle_obj_type() processes the next oBJ_TYPE
static void handle_obj_type() {
	while (peek() != '>' && !is_at_end()) {
		if (peek() == '\n') line++;
		advance();
	}

	if (is_at_end()) {
		error_lexer(line, col, SCAN_EXPECTED_TOKEN, ">");
		return;
	}

	// The closing >.
	advance();

	// Trim the surrounding quotes.
	int s_length = current - 2 - start;
	char value[s_length + 1]; // + 1 for null terminator
	
	memcpy(value, &source[start + 1], s_length);
	value[s_length] = '\0';

	add_token_V(OBJ_TYPE, make_data_str(value));	
}

// handle_string() processes the next string
static void handle_string() {
	while (peek() != '"' && !is_at_end()) {
		if (peek() == '\n') line++;
		advance();
	}

	// Unterminated string.
	if (is_at_end()) {
		error_lexer(line, col, SCAN_UNTERMINATED_STRING);
		return;
	}

	// The closing ".
	advance();

	// Trim the surrounding quotes.
	int s_length = current - 2 - start;
	char value[s_length + 1]; // + 1 for null terminator
	
	memcpy(value, &source[start + 1], s_length);
	value[s_length] = '\0';

	add_token_V(STRING, make_data_str(value));	
}

// identifier() processes the next identifier and also handles wendyScript 
//   keywords
static void identifier() {
	while (is_alpha_numeric(peek())) advance();
	
	char text[current - start + 1];
	memcpy(text, &source[start], current - start);
	text[current - start] = '\0';

	/*if (strcmp(text, "empty") == 0) { add_token_V(NUMBER, make_data_num(0)); }
	else*/ if (strcmp(text, "and") == 0)	{ add_token(AND); }
	else if (strcmp(text, "else") == 0)	{ add_token(ELSE); }
	else if (strcmp(text, "false") == 0)	{ add_token(FALSE); }
	else if (strcmp(text, "if") == 0)	{ add_token(IF); }
	else if (strcmp(text, "or") == 0)	{ add_token(OR); }
	else if (strcmp(text, "true") == 0)	{ add_token(TRUE); }

	else if (strcmp(text, "printstack") == 0)	{ add_token(PRINTSTACK); }
	else if (strcmp(text, "let") == 0)	{ add_token(LET); }
	else if (strcmp(text, "set") == 0)	{ add_token(SET); }
//	else if (strcmp(text, "memset") == 0){ add_token(MEMSET); }
	else if (strcmp(text, "for") == 0)	{ add_token(LOOP); }
	else if (strcmp(text, "none") == 0)	{ add_token(NONE); }
	else if (strcmp(text, "in") == 0)	{ add_token(INOP); }
/*	else if (strcmp(text, "Bool") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "String") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "Number") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "List") == 0)	{ add_token(OBJ_TYPE); }
	else if (strcmp(text, "Address") == 0)	{ add_token(OBJ_TYPE); }*/

//	else if (strcmp(text, "typeof") == 0)	{ add_token(TYPEOF); }


	else if (strcmp(text, "ret") == 0)	{ add_token(RET); }
	else if (strcmp(text, "explode") == 0) { add_token(EXPLODE); }
	else if (strcmp(text, "import") == 0)	{ 
		handle_import();
	}
	else if (strcmp(text, "time") == 0) { add_token(TIME); }
	else if (strcmp(text, "inc") == 0)	{ add_token(INC); }
	else if (strcmp(text, "dec") == 0)	{ add_token(DEC); }
	else if (strcmp(text, "input") == 0)	{ add_token(INPUT); }
	else if (strcmp(text, "struct") == 0){ 
		// HANDLE STRUCT
		//handle_struct(); 
		add_token(STRUCT);
	
	}
	else { add_token(IDENTIFIER); }
}

// handle_number() processes the next number
static void handle_number() {
	while (is_digit(peek())) advance();

	// Look for a fractional part.
	if (peek() == '.' && is_digit(peek_next())) {
		// Consume the "."
		advance();

		while (is_digit(peek())) advance();
	}
	
	char num_s[current - start + 1];
	memcpy(num_s, &source[start], current-start);
	num_s[current - start] = '\0';
	double num = strtod(num_s, NULL); 
	add_token_V(NUMBER, make_data_num(num));
}

static bool ignore_next = false;
// returns true if we sucessfully scanned, false if it was whitespace
static bool scan_token() {
	char c = advance();	
	switch(c) {
		case '(': add_token(LEFT_PAREN); break;
		case ')': add_token(RIGHT_PAREN); break;
		case '[': add_token(LEFT_BRACK); break;
		case ']': add_token(RIGHT_BRACK); break;
		case '{': add_token(LEFT_BRACE); break;
		case '}': add_token(RIGHT_BRACE); break;
		case '&': add_token(AMPERSAND); break;
		case '~': add_token(TILDE); break;
		case ',': add_token(COMMA); break;
		case '.': add_token(DOT); break;
		case '-': 
			if (t_curr == 0 || (is_digit(peek()) && 
					precedence(tokens[t_curr - 1]))) {
				advance();
				handle_number();
			}
			else if (match('=')) {
				add_token(ASSIGN_MINUS);
			}
			else if (match('>')) {
				add_token(RANGE_OP);
			}
			else {
				add_token(MINUS); 
			}
			break;
		case '+': add_token(match('=') ? ASSIGN_PLUS : PLUS); break;
		case '\\': add_token(match('=') ? ASSIGN_INTSLASH : INTSLASH); break;
		case '%': add_token(PERCENT); break;
		case '@': add_token(AT); break;
		case ';': add_token(SEMICOLON); break;
		case ':': add_token(COLON); break;
		case '#': 
			if (match(':')) {
				add_token(LAMBDA);
			}
			else if (match('#')) {
				add_token(DEBUG);
				ignore_next = true;
			}
			else {
				add_token(HASH);
			}
			break;
		case '*': add_token(match('=') ? ASSIGN_STAR : STAR); break;
		case '!': add_token(match('=') ? NOT_EQUAL : NOT); break;
		case '=': 
			if (match('='))
			{
				add_token(EQUAL_EQUAL);
			}
			else if (match('>'))
			{
				add_token(DEFFN);
			}
			else
			{
				add_token(EQUAL);
			}
			break;
		case '<': 
			if (match('=')) {
				add_token(LESS_EQUAL);
			}
			else if (is_alpha(peek())) {
				handle_obj_type();
			}
			else {
				add_token(LESS);
			}
			break;
		case '>': add_token(match('=') ? GREATER_EQUAL : GREATER); break;
		case '/':
			if (match('/')) {
				// A comment goes until the end of the line.
				while (peek() != '\n' && !is_at_end()) advance();
				line++;
				col = 1;
			}
			else if (match('=')) {
				add_token(ASSIGN_SLASH);
			}
			else if (match('*')) {
				while(!(match('*') && match('/'))) {
					if(advance() == '\n') line++;
				}
			}
			else {
				add_token(SLASH);
			}
			break;
		case ' ':
		case '\r':
		case '\t':
			// Ignore whitespace. and commas
			return false;
		case '\n':
			if (!ignore_next) line++;
			else ignore_next = false;
			col = 1;
			return false;
		case '"': handle_string(); break;
		default:
			if (is_digit(c)) {
				handle_number();
			}
			else if (is_alpha(c)) {
				identifier();
			}
			else {
				error_lexer(line, col, SCAN_UNEXPECTED_CHARACTER, c);
			}
			break;
		// END SWITCH HERE
	}
	return true;
}

int scan_tokens(char* source_, token** destination, size_t* alloc_size) {
	source_len = strlen(source_);
	source = safe_malloc(source_len + 1);
	strcpy(source, source_);

	tokens_alloc_size = source_len;
	tokens = safe_malloc(tokens_alloc_size * sizeof(token));
	t_curr = 0;
	current = 0; 
	line = 1;
	col = 1;
	while (!is_at_end()) {
		start = current;
		scan_token();
	}
	*destination = tokens;
	*alloc_size = tokens_alloc_size;
	safe_free(source);
	return t_curr;
}

static void add_token(token_type type) {
	char val[current - start + 1];
	memcpy(val, &source[start], current - start);
	val[current - start] = '\0';
	add_token_V(type, make_data_str(val));
}

static void add_token_V(token_type type, data val) {
	if (type == NONE) { tokens[t_curr++] = none_token(); }
	else if (type == TRUE) { tokens[t_curr++] = true_token(); }
	else if (type == FALSE) { tokens[t_curr++] = false_token(); }
	else {
		token new_t = { type, line, col, val };
		tokens[t_curr++] = new_t;
	}
	if (t_curr == tokens_alloc_size) {
		tokens_alloc_size += 200;
		tokens = safe_realloc(tokens, tokens_alloc_size * sizeof(token));
	}
}

void print_token_list(token* tokens, size_t size) {
	for(int i = 0; i < size; i++) {

		if(tokens[i].t_type == NUMBER) {
			printf("{ %d - %d -> %f }\n", i, 
				tokens[i].t_type, tokens[i].t_data.number);
		}
		else {
			printf("{ %d - %d -> %s }\n", i, 
				tokens[i].t_type, tokens[i].t_data.string);
		}
	}
}

