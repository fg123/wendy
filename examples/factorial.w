// ARRAY MAPPING 

let array_map => (fn, arr, len) {
	let i = 0;
	loop(i < len) {
		set arr[i] = fn(arr[i]);
	};
	ret;
};

let double => (x) {
	ret 2 * x;
};

let my_arr[10] = 2;
set my_arr[0] = 0;
set my_arr[1] = 1;
set my_arr[2] = 2;
set my_arr[3] = 3;
set my_arr[4] = 4;

array_map(double, my_arr, 5);
my_arr[3];
