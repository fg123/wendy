// random.w: WendyScript 1.0
// Random numbers in WendyScript
// By: Felix Guo
// Provides: irand, rand

let r_seed = 1;

let irand => (seed) {
	set r_seed = seed;
	ret;
};

// returns a random number from 0 to max
let rand => (max) {
	set r_seed = (((r_seed * 1103515245) + 12345)) % 2147483647;
	ret r_seed % max; 
};
