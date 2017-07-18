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