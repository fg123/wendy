/* 
 * arrayList.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides auto resizing arrayList
 */

struct arrayList => (list, size, capacity) {
		
	init => () {
		// One less plist allocation than [none] * 4
		this.list = [none, none, none, none];
		this.size = 0;
		this.capacity = 4;
		ret this;
	};

	add => (element) {
		this.list[this.size] = element;
		this.size += 1;
		this.resizeIfNecessary();
	};

	remove => (index) {
		for i in index->this.size
			this.list[i] = this.list[i + 1];
		this.size -= 1;
	};

	resizeIfNecessary => () {
		if this.size >= this.capacity {
			this.list = this.list + ([none] * this.capacity)
			this.capacity *= 2;
		};
	};
}

let <arrayList> [ <number> => (lhs, rhs) {
	ret lhs.list[rhs];
};
