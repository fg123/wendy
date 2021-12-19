// random.w: WendyScript 1.0
// Random numbers in WendyScript
// By: Felix Guo
// Provides: irand, rand

let r_seed = 1;

let irand => (seed) { r_seed = seed; }


// returns a random number from 0 to max
let rand => (max) {
	r_seed = (((r_seed * 1103515245) + 12345)) % 2147483647;
	ret r_seed % max; 
}

struct Random => [float_between, float, int];

Random.float => () native random_float;
Random.float_between => (min, max) this.float() * (max - min) + min;
Random.int => (min, max) (this.float() * (max - min) + min) \ 1;
