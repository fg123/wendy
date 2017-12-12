/* list.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides map(), filter(), sort(), zip()
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

let unique => (list) {
    let sublist = []
    for i in list
        if !(i ~ sublist)
            sublist += i
    ret sublist
};

let zip => (list) {
    let min => (a, b) ret if a < b a else b;
    let minSize = 100000000000000
    for l in list {
        minSize = min(l.size, minSize);
    };
    let result = [[]] * minSize;
    for l in list {
        for j in 0->minSize {
            result[j] += l[j]
        }
    };
    ret result
};

let sum => (list) {
    let a = 0
    for i in list a += i
    ret a
};

let intersect => (list_a, list_b) {
    let result = [];
    for i in list_b
        if i ~ list_a
            result += i
    ret unique(result)
};

let difference => (list_a, list_b) {
    let result = [];
    for i in list_b
        if !(i ~ list_a)
            result += i
    ret unique(result)
};

let expand => (list) {
    let result = [];
    for i in list
        if i.type == <range>
            for j in i result += j
        else result += i
    ret result
};