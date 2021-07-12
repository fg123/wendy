/*
 * linkedlist.w: WendyScript 2.0
 * Created by Felix Guo
 */

struct node => (val, next);
struct linkedlist => (head) {
	init => () {
		this.head = none;
		ret this;
	};

	print => () {
		let curr = this.head;
		let actualList = [];
		for curr != none {
			actualList += curr.val;
			curr = curr.next;
		}
		actualList;
	};

};



