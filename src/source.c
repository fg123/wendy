#include "source.h"
#include "global.h"
#include <string.h>

static char* buffer = 0;
static char** source_lines = 0;
static int max_lines;
static char* source_name;
static bool source_accurate = false;

void init_source(FILE* file, char* name, long length, bool accurate) {
	if (!file) return;
	source_accurate = accurate;
	source_name = safe_malloc(sizeof(char) * (strlen(name) + 1));
	strcpy(source_name, name);
	buffer = safe_malloc(sizeof(char) * (length + 1));
	fread(buffer, sizeof(char), length, file);
	buffer[length] = '\0';

	int lines = 1;
	for (int i = 0; buffer[i]; i++) {
		if (buffer[i] == '\n') {
			// Newline Encountered
			lines++;
		}
	}

	max_lines = lines;
	source_lines = safe_malloc(sizeof(char*) * max_lines);
	int line = 0;
	int line_size = 0;
	char* line_start = buffer;
	for (int i = 0;; i++) {
		line_size++;
		if (buffer[i] == '\n' || buffer[i] == 0) {
			source_lines[line] = safe_malloc(sizeof(char) * line_size);
			strncpy(source_lines[line], line_start, line_size);
			source_lines[line][line_size - 1] = 0;
			line_start = buffer + i + 1;
			line++;
			line_size = 0;
		}
		if (!buffer[i]) break;
	}
}

bool is_source_accurate() {
	return source_accurate;
}
bool is_valid_line_num(int line) {
	// line is 1 indexed
	return line <= max_lines;
}

bool has_source() {
	return buffer;
}

char* get_source_name() {
	return source_name;
}

char* get_source_line(int line) {
	if (line >= max_lines) {
		return "";
	}
	return source_lines[line - 1];
}

char* get_source_buffer() {
	return buffer;
}

void free_source() {
	if (!buffer) return;
	safe_free(buffer);
	safe_free(source_name);
	for (int i = 0; i < max_lines; i++) {
		if (source_lines[i]) {
			safe_free(source_lines[i]);
		}
	}
	safe_free(source_lines);
}

