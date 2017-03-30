#include "tests.h"
#include <stdio.h>
#include "memory.h"

void mem_block_test() {	
	print_free_memory();
	// Request block of 100
	pls_give_memory(100);
	print_free_memory();
	pls_give_memory(20);
	print_free_memory();

	here_u_go(10, 10);
	print_free_memory();
	here_u_go(25, 95);
	print_free_memory();
}

void run_tests() {
//	mem_block_test();	
}
