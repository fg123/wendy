#ifndef EXECPATH_H
#define EXECPATH_H

// execpath.h - Felix Guo
// Provides a cross-platform way to get the path of the executable

// get_path() returns a malloced string containing the path to the executable
char* get_path(void);

#endif
