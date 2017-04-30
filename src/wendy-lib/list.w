// list.w: WendyScript 1.0
// List functions in WendyScript,
// Based on Racket-style lists
// By: Felix Guo
// Provides: map, filter, fold, first, for and rest

let first => (list) {
	@"First: ";
	list;
	ret list[0];
};

let for => (list, fn) {
	let x = 0;
	loop (x < list.size) {
		fn(list[x]);
		inc x;
	};
	ret;
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
	if (list.size == 0) {
		ret [];
	}
	else if (fn(first(list))) {
		ret [first(list)] + filter(fn, rest(list));
	}
	else {
		ret filter(fn, rest(list));
	};
};

let sort => (list) {
	let insert => (elem, list) {
		let result = [];
		let i = 0;
		let added = false;
		loop (i < list.size) {
			if (elem < list[i] and !added) {
				set result = result + elem;
				set added = true;
			};
			set result = result + list[i];
			inc i;
		};
		if (!added) {
			set result = result + elem;
		};
		ret result;
	};
  	let result = [];
  	let i = 0;
  	loop (i < list.size) {
  		set result = insert(list[i], result);
    	inc i;
  	};
	ret result;
};