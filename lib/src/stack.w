/*
 * stack.w: WendyScript 2.0
 * Created by Felix Guo
 */
struct node => (v, next);
struct stack => (head) [push, pop, top, empty];
stack.init => () {
	this.head = none;
	ret this;
};

stack.push => (item) {
	let new = node(item, this.head);
	this.head = new;
};

stack.empty => () {
	ret this.head == none;
};

stack.pop => () {
	this.head = this.head.next;
};

stack.top => () {
	ret this.head.v;
};
