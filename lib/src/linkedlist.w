/*
 * linkedlist.w: WendyScript 2.0
 * Created by Felix Guo
 */

struct node => (val, next);
struct linkedlist => (head) [print, add, remove, init];
linkedlist.init => () {
	this.head = none;
	ret this;
};

linkedlist.print => () {
	let curr = this.head;
	let actualList = [];
	for curr != none {
		actualList += curr.val;
		curr = curr.next;
	}
	actualList;
};





