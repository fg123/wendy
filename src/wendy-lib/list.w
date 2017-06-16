/* list.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides map(), filter(), sort()
 */

let map => (fn, list) {
	if (list.size == 0) {
		ret [];
	}
	else {
		ret [fn(list[0])] + map(fn, list[1->list.size]);
	};
};

let filter => (fn, list) {
	if (list.size == 0) {
		ret [];
	}
	else if (fn(first(list))) {
		ret [list[0]] + filter(fn, list[1->list.size]);
	}
	else {
		ret filter(fn, list[1->list.size]);
	};
};

let sort => (list) {
	let insert => (elem, list) {
		let result = [];
		let added = false;
		for i:0->list.size {
			if (elem < list[i] and !added) {
				result += elem;
				added = true;
			};
			result += list[i];
			inc i;
		};
		if (!added) {
			result += elem;
		};
		ret result;
	};
  	let result = [];
  	let i = 0;
  	for (i < list.size) {
  		result = insert(list[i], result);
    	inc i;
  	};
	ret result;
};
