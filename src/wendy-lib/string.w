/*
 * string.w: WendyScript 2.0
 * String Functions for WendyScript
 * By: Felix Guo
 * Provides: split, join, % substitution
 */

import list;
import data;

let <string> / <string> => (lhs, rhs) {
    // Split LHS with RHS
    let res = [];
    let last = 0;
    for i in 0->lhs.size {
        if (rhs == lhs[i]) {
            res += lhs[last->i];
            last = i + 1;
        }
    }
    if (last != lhs.size) {
        res += lhs[last->lhs.size];
    }
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
    let parts = lhs / " ";
    for i in 0->rhs.size {
        let search = "$" + (i + 1);
        parts = map(#:(x) {
            if (x == search) ret rhs[i];
            else ret x;
        }, parts);
    }
    ret parts % " ";
};