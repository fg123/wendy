/*
 * stack.w: WendyScript 2.0
 * Created by Felix Guo
 */
struct node => (v, next);
struct stack => (head) {
	init => () {
		this.head = none;
		ret this;
	};

	push => (item) {
		let new = node(item, this.head);
		this.head = new;
	};

	empty => () {
		ret this.head == none;
	};

	pop => () {
		this.head = this.head.next;
	};

	top => () {
		ret this.head.v;
	};
};