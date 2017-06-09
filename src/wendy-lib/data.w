/* data.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides int(), str() and digit() 
 */

let digit => (str) {
	if (str == "1") 	 ret 1;
	else if (str == "2") ret 2;
	else if (str == "3") ret 3;
	else if (str == "4") ret 4;
	else if (str == "5") ret 5;
	else if (str == "6") ret 6;
	else if (str == "7") ret 7;
	else if (str == "8") ret 8;
	else if (str == "9") ret 9;
	else ret 0;
};
let int => (str) {
	let num = 0;
	let i = 0;
	for (i < str.size) {
		num = (num * 10) + digit(str[i]);		
		inc i;
	};
	ret num;
};
let str => (num) {
	ret num + "";
};