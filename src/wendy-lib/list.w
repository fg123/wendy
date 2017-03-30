// list.w: WendyScript 1.0
// List functions in WendyScript,
// Based on Racket-style lists
// By: Felix Guo
// Provides: map, filter, fold, first and rest

let first => (list) {
	@"First: ";
	list;
	ret list[0];
};

let rest => (list) {
	let new[list.size - 1];
	let x = 1;
	loop (x < list.size) {
		set new[x - 1] = list[x];
		inc x;
	};
	ret new;
};
// fn must be a 2 argument function
let fold => (fn, list, initial) {
	let result = initial;
	loop (!is_empty(list)) {
		set result = fn(first(list), result);
		set list = rest(list);
	};
	ret result;
};

let map => (fn, list) {
	if (list.size == 0) {
		ret [];
	}
	else {
		ret [fn(first(list))] + map(fn, rest(list));
	};
};

let filter => (fn, list) {
	list;
	first(list);
	list.size;
	if ((list.size) == 0) {
		"WHY DONT I COME IN HERE";
		ret [];
	}
	else if (fn(first(list))) {
		ret [first(list)] + filter(fn, rest(list));
	}
	else {
		ret filter(fn, rest(list));
	};
};