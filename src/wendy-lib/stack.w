/* 
 * stack.w: WendyScript 2.0
 * Created by Felix Guo
 */
struct node => (val, next);
struct stack => (head) [push, pop, top];
stack.init => () {
    this.head = none;
    ret this;
};

stack.push => (item) {
    let new = node(item, this.head);
    this.head = new;
};

stack.pop => () {
    this.head = this.head.next;
};

stack.top => () {
    ret this.head.val;
};
