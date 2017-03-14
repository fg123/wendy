#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

// Preprocessor processes the function calls and rewrites them for the Wendy
//   Interpreter. Let's take a look at function calls and how they are 
//   optimized.
//
// Function headers, aka the (arg1, arg2, ...) { ... }; is changed to simply
//   { ... }. Arguments are removed and placed into the function, retreived 
//   from the stack. Let's consider this example:
//   
//     let f => (x, y) { ret x + y; };
//     f(30, f(f(40, 50), f(60, 70)));
//
//   The header becomes: 
//		
//		let f => { let y; pop y; let x; pop x; ret x + y; };
//
//	 And the function call becomes:
//
//		push 40;
//		push 50;
//		call f; 
//		let ~tmp1;
//		pop ~tmp1; <- f(40, 50)
//		push 60;
//		push 70;
//		call f;
//		let ~tmp2;
//		pop ~tmp2; <- f(60, 70)
//		push ~tmp1; 
//		push ~tmp2;
//		call f;   
//		let ~tmp3;
//		pop ~tmp3;  <- f(f(40,50), f(60, 70))
//		push 30;
//		push ~tmp3;
//		call f;
//		let ~tmp4;  
//		pop ~tmp4;  <- f(30, ...)
//		~tmp4;
//		del ~tmp1;
//		del ~tmp2;
//		del ~tmp3;
//		del ~tmp4;
//
// Arguments to functions are pushed onto the stack from the front, then are
//   popped in the function in reverse order. Temporary values are stored in
//   variables beginning with ~ (which is not a valid character for user
//   defined variables.

#include <stdbool.h>
#include "token.h"
#include <stdlib.h>

extern char* p_dump_path;

// preprocess(tokens, length, alloc_size) takes in a pointer to the pointer to 
//   the malloc'd block of tokens and preprocesses them, returning the new size
//   of the token list.
// effects: malloc'd block may become bigger via realloc.
//          original pointer may change
size_t preprocess(token** _tokens, size_t _length, size_t _alloc_size);


#endif
