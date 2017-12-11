/* math.w: WendyScript 2.0
 * Basic Math Functions for WendyScript
 * By: Felix Guo
 * Provides: factorial, pow, sqrt, root, is_even, is_odd
 */
let abs => (x) if x < 0 ret -x else ret x

let factorial => (x)
    if(x < 1) ret 0;
    else if(x > 1) ret x * factorial(x - 1);
    else ret 1;

let pow => (base, expt) native pow;
let log10 => (x) native log;
let ln => (x) native ln;
let log => (x, base) ln(x) / ln(base);
let root => (num, rt) pow(num, 1 / rt);
let sqrt => (n) root(n, 2);
let is_even => (x) x % 2 == 0
let is_odd => (x) x % 2 == 1
