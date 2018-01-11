/* print.w: WendyScript 2.0
 * Print Functions for WendyScript
 * By: Felix Guo
 * Provides: print_padded(str, size, padwith)
 */

let print_padded => (str, size, padwith) {
    // Convert to String if isn't
    let print_str = str + "";
    if print_str.size > size {
        // Truncate and Print
        ret print_str[0->size];
    }
    else if print_str.size < size {
        ret ((padwith + "") * (size - print_str.size)) + print_str;
    }
    ret print_str;
};
