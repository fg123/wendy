struct cons => (value, next);

// Getting Components
let first => (list) {
	ret cons_value(list);
};
let rest => (list) {
	ret cons_next(list);
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
	ret "";
};

let insert => (element, lst) {
	if (lst == none) {
		ret cons(element, none);
	}
	else if (element <= first(lst)) {
		ret cons(element, lst);
	}
	else {
		ret cons(first(lst), insert(element, rest(lst)));
	};
};

// lst is a linked list
let sort => (lst) {
	if (lst == none) {
		ret none;
	}
	else {
		ret insert(first(lst), sort(rest(lst)));
	};
};



// Prompt User for Numbers
@"How many numbers to sort? ";
let num_count;
input num_count;

let numbers = none;
let i = 0;
loop (i < num_count) {
	@"Enter Number " + i + ": ";
	let inp;
	input inp;
	set numbers = cons(inp, numbers);
	inc i;
};

let b = sort(numbers);
@"The sorted list: ";
loop (b != none) {
	@first(b) + " ";
	set b = rest(b);
};
" ";


