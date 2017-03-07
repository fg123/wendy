// wmath.w: WendyScript 1.0
// Basic Math Functions for WendyScript
// By: Felix Guo
// Provides: factorial, pow, sqrt, root, is_even, is_odd

let factorial => (x)  {
	if(x < 1) {
		err "factorial: input must be greater than 0";
		ret;
	}
	else if(x > 1) {
		ret x * factorial(x - 1);
	}
	else {
		ret 1;
	};
};

let pow => (base, expt) {
	if(expt < 0) {
		ret 1 / pow(base, -expt);
	}
	else if (expt == 0) {
		ret 1;
	}
	else if (expt % 2 == 0) {
        let half_pow = pow(base, expt / 2);
        ret half_pow * half_pow;
    }
	else {
		ret base * pow(base, expt - 1);
	};
};

let root => (num, rt) {
	let next_it => (x, rt, n) {
		// top is x ^ rt - orig, bottom is derivative of it
		ret (x - ((pow(x, rt) - n) / (rt * pow(x, (rt - 1)))));
	};
	let ans = 1;
	let x = next_it(ans, rt, num);

	loop (ans != x) {
		set ans = x;
		set x = next_it(ans, rt, num);
	};
	ret x;
};
let sqrt => (n) {
	ret root(n, 2);
};
let abs => (n) {
	if(n < 0) {
		ret -n;
	}
	else {
		ret n;
	};
};
let is_even => (x) {
	if (x % 2 == 0) {
		ret true;
	}
	else {
		ret false;
	};
};
let is_odd => (x) {
	if (x % 2 == 1) {
		ret true;
	}
	else {
		ret false;
	};
};