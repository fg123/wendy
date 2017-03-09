// list.w: WendyScript 1.0
// Linked-list implementation in WendyScript,
// Based on Racket-style lists
// By: Felix Guo
// Provides: cons, first, rest, length, build_list, print_list, empty
// Also provides abstract list functions: foldr, foldl


// Each cons contains a value and a pointer to the next node.
// empty is used to refer to a pointer to the none object

struct cons => (value, next);

// Getting Components
let first => (list) {
	ret cons_value(list);
};
let rest => (list) {
	ret cons_next(list);
};

let is_empty => (list) {
	ret list == none;
};
// Length of List
let length => (list) {
	let size = 0;
	loop (list != none) {
		inc size;
		set list = rest(list);
	};
	ret size;
};

// Builds a list from 1 to len
let build_list => (len) {
	let res = none;
	loop (len > 0) {
		set res = cons(len, res);
		dec len;
	};
	ret res;
};

// Prints the List
let print_list => (list) {
	@"List: ";
	loop (list != none) {
		@first(list) + " ";
		set list = rest(list);
	};
	"";
	ret;
};

let foldr => (fn, list, initial) {
	if (is_empty(list)) {
		ret initial;
	}
	else {
		ret fn(first(list), foldr(fn, rest(list), initial));
	};
};
// fn must be a 2 argument function
let foldl => (fn, list, initial) {
	let result = initial;
	loop (!is_empty(list)) {
		set result = fn(first(list), result);
		set list = rest(list);
	};
	ret result;
};

let map => (fn, list) {
	if (is_empty(list)) {
		ret none;
	}
	else {
		ret cons(fn(first(list)), map(fn, rest(list)));
	};
};

let filter => (fn, list) {
	if (is_empty(list)) {
		ret none;
	}
	else if (fn(first(list))) {
		ret cons(first(list), filter(fn, rest(list)));
	}
	else {
		ret filter(fn, rest(list));
	};
};