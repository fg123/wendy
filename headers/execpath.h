#ifndef EXECPATH_H
#define EXECPATH_H

// execpath provides a unix + windows way to get the path of the executable

// get_path returns a malloced string containing the path to the executable
char* get_path();

#endif
