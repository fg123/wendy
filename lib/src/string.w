/*
 * string.w: WendyScript 2.0
 * String Functions for WendyScript
 * By: Felix Guo
 * Provides: split, join, % substitution
 */

import list;
import data;

let trim => (str) {
	let whitespace = ["\n", "\r", " ", "\t", "\v", "\f"];
	let i = 0;
	for i < str.size {
		if (str[i] ~ whitespace) inc i;
		else break;
	}
	let j = str.size - 1;
	for j >= 0 {
		if (str[j] ~ whitespace) dec j;
		else break;
	}
	if (i > j) ret "";
	let r = "";
	for k in i->(j + 1) {
		r += str[k];
	}
	ret r;
};

let <string> / <string> => (lhs, rhs) {
	// Split LHS with RHS
	let res = [];
	let last = 0;
	for i in 0->(lhs.size - rhs.size + 1) {
		if (rhs == lhs[i->(i + rhs.size)]) {
			res += lhs[last->i];
			i += rhs.size;
			last = i;
		}
	}
	if (last != lhs.size - rhs.size + 1) {
		res += lhs[last->lhs.size];
	}
	ret res;
	ret filter(#:(x) x != "", res);
};

let <list> % <string> => (lhs, rhs) {
	let res = "";
	for i in 0->lhs.size {
		if i != 0 res += rhs;
		res += str(lhs[i]);
	}
	ret res;
};

let <string> % <list> => (lhs, rhs) {
	let parts = lhs / "$";
	for i in 0->rhs.size {
		let search = (i + 1) + "";
		parts = map(#:(x) {
			if (x[0->search.size] == search)
				ret rhs[i] + x[search.size -> x.size];
			else ret x;
		}, parts);
	}
	ret parts % "";
};
