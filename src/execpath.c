#include "execpath.h"
#include <stdio.h>
#include <limits.h>
#include "global.h"

#define W_MAX_PATH 500
#ifdef _WOP_IN32

#include <Windows.h>

char* get_path() {
    HMODULE module = GetModuleHandleA(NULL);
    char* path = safe_malloc(W_MAX_PATH);
    int end = GetModuleFileNameA(module, path, W_MAX_PATH);
    while (path[end] != '/' && path[end] != '\\') {
        end--;
    }
    path[end + 1] = 0;
    return path;
}
#elif __APPLE__
#include <mach-o/dyld.h>
#include <stdio.h>
#include <string.h>

char* get_path() {
    char* path = safe_malloc(W_MAX_PATH);
    uint32_t size = W_MAX_PATH;
    if(_NSGetExecutablePath(path, &size) == 0) {
        size_t end = strlen(path);
        while (path[end] != '/' && path[end] != '\\') {
            end--;
        }
        path[end + 1] = 0;
        return path;
    }
    else {
        printf("Path is too long. Required %u bytes.\n", size);
        safe_exit(1);
    }
    return 0;
}
#else
#undef __POSIX_VISIBLE
#define __POSIX_VISIBLE 200112
#include <unistd.h>
#include <sys/types.h>

ssize_t readlink(const char *path, char *buf, size_t bufsiz);

char* get_path() {
    char* path = safe_malloc(W_MAX_PATH);
    int end = readlink("/proc/self/exe", path, W_MAX_PATH);
    while (path[end] != '/' && path[end] != '\\') {
        end--;
    }
    path[end + 1] = 0;
    return path;
}


#endif
