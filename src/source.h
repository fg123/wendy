#ifndef SOURCE_H
#define SOURCE_H

#include <stdbool.h>
#include <stdio.h>

// source.h - Felix Guo
// Manages source file input if available.

// init_source(file) given a file pointer, initializes the source module 
//   provide 0 as the file pointer if there is no source file supplied
void init_source(FILE* file, const char* name, long length, bool accurate);

// has_source() returns true if there is a source file, false if there isn't
bool has_source(void);

// get_line(line) returns a pointer to a null terminated line of the source
//   file
char* get_source_line(int line);

// get_buffer() returns a pointer to the buffer holding the contents of the
//   source file
char* get_source_buffer(void);

// get_source_name() returns the name loaded as the source.
char* get_source_name(void);

// is_valid_line_num(line) returns true if it's within the range and false
//   otherwise.
bool is_valid_line_num(int line);

// is_source_accurate() returns true if the source was directly loaded and 
//   false otherwise.
bool is_source_accurate(void);

// free_source() clears all memory used by the source module.
void free_source(void);

#endif
