/* data.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides int(), str() and digit() 
 */

let int => (s) native stringToInteger;
let str => (n) n + "";
let hash => (n) 
  if n.type == <number> ret n
  else if n.type == <string> {
    let h = 7
    for i in n h += h * 31 + i.val
    ret h
  }
  else {
    ret 
  }
let reverse => (str) native reverseString;