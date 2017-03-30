#include "execpath.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define W_MAX_PATH 500
#ifdef _WIN32

#include <Windows.h>

char* get_path() {
	HMODULE module = GetModuleHandleA(NULL);
	char* path = malloc(W_MAX_PATH);
	int end = GetModuleFileNameA(module, path, W_MAX_PATH);
	while (path[end] != '/' && path[end] != '\\') {
		end--;
	}
	path[end + 1] = 0;
	return path;
}

#else
#undef __POSIX_VISIBLE
#define __POSIX_VISIBLE 200112
#include <unistd.h>
#include <sys/types.h>

ssize_t readlink(const char *path, char *buf, size_t bufsiz);

char* get_path() {
	char* path = malloc(W_MAX_PATH);
	int end = readlink("/proc/self/exe", path, W_MAX_PATH);
	while (path[end] != '/' && path[end] != '\\') {
		end--;
	}
	path[end + 1] = 0;
	return path;
}


#endif
