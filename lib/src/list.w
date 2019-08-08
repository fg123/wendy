/*
 * list.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides map(), filter(), sort(), zip()
 */

let map => (fn) {
  // Arguments are lists of lists.
  if (arguments.size == 0) {
  	ret [];
  }
  let size = arguments[0].size;
  for lst in arguments {
  	if size != lst.size {
  		"Sizes must all be the same!";
  		ret [];
  	}
  }
  let out = [];
  if (arguments.size == 1) {
  	// Fast Path
  	for i in arguments[0]
  		out += fn(i);
  }
  else {
  	// Construct
  	for i in 0->size {
  		out += fn(...map(#:(l) l[i], arguments));
  	}
  }
  ret out;
}


let filter => (fn, list)
	if list.size == 0 ret [];
	else if fn(list[0])
		ret [list[0]] + filter(fn, list[1->list.size]);
	else
		ret filter(fn, list[1->list.size]);


let sort => (list) {
	let insert => (elem, list) {
		let result = [];
		let added = false;
		for i:list {
			if elem < i and !added {
				result += elem;
				added = true;
			};
			result += i;
		};
		if !added result += elem
		ret result;
	};
  let result = [];
  for i:list result = insert(i, result)
	ret result;
};

let unique => (list) {
	let sublist = []
	for i in list
		if !(i ~ sublist)
			sublist += i
	ret sublist
};

let zip => (list) {
	let min => (a, b) ret if a < b a else b;
	let minSize = 100000000000000
	for l in list {
		minSize = min(l.size, minSize);
	};
	let result = [[]] * minSize;
	for l in list {
		for j in 0->minSize {
			result[j] += l[j]
		}
	};
	ret result
};

let sum => (list) {
	if list.size == 0 ret 0;
	let add => (l, r) l + r;
	ret reduce(list[0->(list.size - 1)], add, list[list.size - 1]);
};

let intersect => (list_a, list_b) {
	let result = [];
	for i in list_b
		if i ~ list_a
			result += i
	ret unique(result)
};

let difference => (list_a, list_b) {
	let result = [];
	for i in list_b
		if !(i ~ list_a)
			result += i
	ret unique(result)
};

let reduce => (list, fn, initial)
	if list.size == 0 ret initial
	else ret fn(list[0], reduce(list[1->list.size], fn, initial))

let indexOf => (list, item) {
	for i in 0->list.size
		if list[i] == item ret i;
	ret -1;
};

let @ <list> => (lst) {
	@"[";
	for i in 0->lst.size {
		if (i != 0) @", ";
		@lst[i];
	}
	ret "]";
};
