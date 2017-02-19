#include "error.h"
#include <stdio.h>

// error handling functions
void error(int line, char* message) {
	printf("Runtime Error at line %d: %s\n", line, message);
}
