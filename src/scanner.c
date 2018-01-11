#include "scanner.h"
#include "error.h"
#include "global.h"
#include "execpath.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static size_t source_len;
static size_t current; // is used to keep track of source current
static size_t start;
static char* source;
static token* tokens;
static size_t tokens_alloc_size;
static size_t t_curr; // t_curr is used to keep track of addToken
static size_t line;
static size_t col;

static bool scan_token();
static void add_token(token_type type);
static void add_token_V(token_type type, token_data val);

static bool is_at_end() {
    return current >= source_len;
}

static bool match(char expected) {
    if (is_at_end()) return false;
    if (source[current] != expected) return false;
    col++;
    current++;
    return true;
}

static char peek() {
    if (is_at_end()) return '\0';
    return source[current];
}

static char advance() {
    current++;
    col++;
    return source[current - 1];
}

static char peek_next() {
    if(current + 1 >= source_len) return '\0';
    return source[current + 1];
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha_numeric(char c) {
    return is_alpha(c) || is_digit(c);
}

// handle_import() imports the string contents of the given file, otherwise
//   leaves it for code_gen
static void handle_import() {
    add_token(T_REQ);
    // we scan a string
    // t_curr points to space after REQ
    start = current;
    while(!scan_token()) {
        start = current;
    }

    if (tokens[t_curr - 1].t_type == T_STRING) {
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
    }
}

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

    add_token_V(T_OBJ_TYPE, make_data_str(value));
}

static void handle_string(char endChar) {
    while (peek() != endChar && !is_at_end()) {
        if (peek() == '\n') line++;
        if (peek() == '\\') advance();
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
    size_t s_length = current - 2 - start;
    char value[s_length + 1]; // + 1 for null terminator
    memcpy(value, &source[start + 1], s_length);
    value[s_length] = '\0';
    size_t s = 0;
    for (size_t i = 0; i < s_length; i++) {
        if (value[i] == '\\' && i != s_length - 1) {
            switch(value[++i]) {
                case 'n':
                    value[s++] = '\n';
                    break;
                case 't':
                    value[s++] = '\t';
                    break;
                default:
                    value[s++] = value[i];
            }
        }
        else {
            value[s++] = value[i];
        }
    }
    value[s] = 0;
    add_token_V(T_STRING, make_data_str(value));
}

// identifier() processes the next identifier and also handles wendyScript
//   keywords
static void identifier() {
    while (is_alpha_numeric(peek())) advance();

    char text[current - start + 1];
    memcpy(text, &source[start], current - start);
    text[current - start] = '\0';

    if (streq(text, "and"))   { add_token(T_AND); }
    else if (streq(text, "else")) { add_token(T_ELSE); }
    else if (streq(text, "false"))    { add_token(T_FALSE); }
    else if (streq(text, "if"))   { add_token(T_IF); }
    else if (streq(text, "or"))   { add_token(T_OR); }
    else if (streq(text, "true")) { add_token(T_TRUE); }

    else if (streq(text, "let"))  { add_token(T_LET); }
    else if (streq(text, "set"))  { add_token(T_SET); }
    else if (streq(text, "for"))  { add_token(T_LOOP); }
    else if (streq(text, "none")) { add_token(T_NONE); }
    else if (streq(text, "in"))   { add_token(T_IN); }

    else if (streq(text, "ret"))  { add_token(T_RET); }
    else if (streq(text, "import"))   {
        handle_import();
    }
    else if (streq(text, "native"))   {
        add_token(T_NATIVE);
    }
    else if (streq(text, "inc"))  { add_token(T_INC); }
    else if (streq(text, "dec"))  { add_token(T_DEC); }
    else if (streq(text, "input"))    { add_token(T_INPUT); }
    else if (streq(text, "struct")){
        add_token(T_STRUCT);

    }
    else { add_token(T_IDENTIFIER); }
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
    add_token_V(T_NUMBER, make_data_num(num));
}

static bool ignore_next = false;
// returns true if we sucessfully scanned, false if it was whitespace
static bool scan_token() {
    char c = advance();
    switch(c) {
        case '(': add_token(T_LEFT_PAREN); break;
        case ')': add_token(T_RIGHT_PAREN); break;
        case '[': add_token(T_LEFT_BRACK); break;
        case ']': add_token(T_RIGHT_BRACK); break;
        case '{': add_token(T_LEFT_BRACE); break;
        case '}': add_token(T_RIGHT_BRACE); break;
        case '&': add_token(T_AND); break;
        case '|': add_token(T_OR); break;
        case '?': add_token(T_IF); break;
        case '~': add_token(T_TILDE); break;
        case ',': add_token(T_COMMA); break;
        case '.': add_token(T_DOT); break;
        case '-':
            if (t_curr == 0 || (is_digit(peek()) &&
                    precedence(tokens[t_curr - 1]))) {
                advance();
                handle_number();
            }
            else if (match('=')) {
                add_token(T_ASSIGN_MINUS);
            }
            else if (match('-')) {
                add_token(T_DEC);
            }
            else if (match('>')) {
                add_token(T_RANGE_OP);
            }
            else {
                add_token(T_MINUS);
            }
            break;
        case '+':
            if (match('=')) {
                add_token(T_ASSIGN_PLUS);
            }
            else if (match('+')) {
                add_token(T_INC);
            }
            else {
                add_token(T_PLUS);
            }
            break;
        case '\\': add_token(match('=') ? T_ASSIGN_INTSLASH : T_INTSLASH); break;
        case '%': add_token(T_PERCENT); break;
        case '@': add_token(T_AT); break;
        case ';': add_token(T_SEMICOLON); break;
        case ':': add_token(T_COLON); break;
        case '#':
            if (match(':')) {
                add_token(T_LAMBDA);
            }
            else if (match('#')) {
                add_token(T_DEBUG);
                ignore_next = true;
            }
            else {
                add_token(T_LOOP);
            }
            break;
        case '*': add_token(match('=') ? T_ASSIGN_STAR : T_STAR); break;
        case '!': add_token(match('=') ? T_NOT_EQUAL : T_NOT); break;
        case '=':
            if (match('='))
            {
                add_token(T_EQUAL_EQUAL);
            }
            else if (match('>'))
            {
                add_token(T_DEFFN);
            }
            else
            {
                add_token(T_EQUAL);
            }
            break;
        case '<':
            if (match('=')) {
                add_token(T_LESS_EQUAL);
            }
            else if (is_alpha(peek())) {
                handle_obj_type();
            }
            else if (match('<')) {
                add_token(T_LET);
            }
            else {
                add_token(T_LESS);
            }
            break;
        case '>':
            if (match('=')) {
                add_token(T_GREATER_EQUAL);
            }
            else if (match('>')) {
                add_token(T_INPUT);
            }
            else {
                add_token(T_GREATER);
            }
            break;
        case '/':
            if (match('/')) {
                // A comment goes until the end of the line.
                while (peek() != '\n' && !is_at_end()) advance();
                line++;
                col = 1;
            }
            else if (match('>')) {
                add_token(T_RET);
            }
            else if (match('=')) {
                add_token(T_ASSIGN_SLASH);
            }
            else if (match('*')) {
                while(!(match('*') && match('/'))) {
                    if(advance() == '\n') line++;
                }
            }
            else {
                add_token(T_SLASH);
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
        case '"': handle_string('"'); break;
		case '$': add_token(T_DOLLAR_SIGN); break;
        case '\'': handle_string('\''); break;
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
        // OP_END SWITCH HERE
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

static void add_token_V(token_type type, token_data val) {
    if (type == T_NONE) { tokens[t_curr++] = none_token(); }
    else if (type == T_TRUE) { tokens[t_curr++] = true_token(); }
    else if (type == T_FALSE) { tokens[t_curr++] = false_token(); }
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
    for(size_t i = 0; i < size; i++) {

        if (tokens[i].t_type == T_NUMBER) {
            printf("%zd - %s -> %f\n", i,
                token_string[tokens[i].t_type], tokens[i].t_data.number);
        }
        else {
            printf("%zd - %s -> %s\n", i,
                token_string[tokens[i].t_type], tokens[i].t_data.string);
        }
    }
}

