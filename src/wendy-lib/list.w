/* list.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides map(), filter(), sort()
 */

let map => (fn, list)
  if list.size == 0
    ret []
  else 
    ret [fn(list[0])] + map(fn, list[1->list.size])
  

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