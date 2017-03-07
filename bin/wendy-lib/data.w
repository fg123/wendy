// data.w: WendyScript 1.0
// Data manipulation in WendyScript
// By: Felix Guo
// Provides: strtonum, numtostr, is_digit, substring

let substring => (str, start, length) {
	explode str;
	let i = 0;
	let res = "";
	loop (i < length) {
		set res = res + str[i + start];
		inc i;
	};
	ret res;
};
let digit => (str) {
	if (str == "1") {
		ret 1;
	}
	else if (str == "2") {
		ret 2;
	}
	else if (str == "3") {
		ret 3;
	}
	else if (str == "4") {
		ret 4;
	}
	else if (str == "5") {
		ret 5;
	}
	else if (str == "6") {
		ret 6;
	}
	else if (str == "7") {
		ret 7;
	}
	else if (str == "8") {
		ret 8;
	}
	else if (str == "9") {
		ret 9;
	}
	else {
		ret 0;
	};
};
let strtonum => (str) {
	explode str;
	let num = 0;
	let i = 0;
	loop (str[i] != none) {
		set num = num * 10;
		set num = num + digit(str[i]);
		inc i;
	};
	ret num;
};
let numtostr => (num) {
	ret num + "";
};
let is_digit => (str) {
	if (str == "1") {
		ret true;
	}
	else if (str == "2") {
		ret true;
	}
	else if (str == "3") {
		ret true;
	}
	else if (str == "4") {
		ret true;
	}
	else if (str == "5") {
		ret true;
	}
	else if (str == "6") {
		ret true;
	}
	else if (str == "7") {
		ret true;
	}
	else if (str == "8") {
		ret true;
	}
	else if (str == "9") {
		ret true;
	}
	else if (str == "0") {
		ret true;
	}
	else {
		ret false;
	};
};

