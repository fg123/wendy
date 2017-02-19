let fn_wrap => (fn_str) {
	let sum => (x, y) { ret x + y; };
	let diff => (x, y) { ret x - y; };

	if (fn_str == "sum") { 
		ret sum; 
	}
	else { ret diff; };
};
let x = fn_wrap("sum");
x(10, 20);
fn_wrap("diff")(10, 20);



