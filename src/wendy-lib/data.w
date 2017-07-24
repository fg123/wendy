/* data.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides int(), str() and digit() 
 */

let digit => (s) s.val - "0".val;
let int => (s) 
  if s.size != 0 
    ret digit(s[0]) + int(s[1->s.size]) 
  else ret "";
let str => (n) n + "";
let hash => (n) 
  if n.type == <number> ret n
  else if n.type == <string> {
    let h = 7
    for i:str h += h * 31 + i.val
    ret h
  }
  else {
    ret 
  }
let reverse => (str) {
  let a = ""
  for i: str.size->0 a += str[i-1]
  ret a
}