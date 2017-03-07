// sort.w: WendyScript 1.0
// Insertion Sort Algorithm on a Linked List
// By: Felix Guo
// Provides: sort

req "list.w";

let insert => (element, lst) {
	if (lst == empty) {
		ret cons(element, empty);
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
	if (lst == empty) {
		ret empty;
	}
	else {
		ret insert(first(lst), sort(rest(lst)));
	};
};


