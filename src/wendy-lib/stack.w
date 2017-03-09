// stack.w: WendyScript 1.0
// Stack Implementation in WendyScript
// By: Felix Guo
// Provides: stack, push, pop, top, is_empty

// stack() creates a pointer to the end of the stack / empty
let stack => () {
	let x = none;
	ret &x;
};

// push(val, stk) adds an element to the top of the stack and
//   sets the top of the stk to be that element.
// NOTE: modifies stk in place.
let push => (val, stk) {
	let arr[2];
	set arr[0] = val;
	set arr[1] = *stk;
	memset stk = &arr;
	ret;
};

// pop(stk) removes the top element of the stack (by setting the top
//   of the stack to the next element)
let pop => (stk) {
	//stk is address to stack which is a address to the first element
	memset stk = *(*stk + 1);
	ret;
};

// top(stk) returns the value of the top element of the stack.
let top => (stk) {
	if((*stk) == none) {
		"Stack is Empty";
		ret none;
	}
	else {
		ret *stk;
	};
};

// is_empty(stk) returns true if the stack is empty and false 
//   otherwise.
let is_empty => (stk) {
	ret (*stk) == none;
};