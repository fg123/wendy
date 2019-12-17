/**
 * MessageBus library for WendyScript, the only way
 *   to do parallel processing.
 * 
 * This high level interface is support underneath by
 *   the VM, as it needs to handle thread locking
 *   procedures.
 */

struct Bus => (name) [register, post, aggregate, split];

Bus.register => (fn /* accepting types */) {
    let register => (name, fn, args) native bus_register;
    ret register(this.name, fn, arguments);
};

Bus.post => (msg) {
    let post => (name, msg) native bus_post;
    ret post(this.name, msg);
};

Bus.aggregate => () {
	for bus in arguments {
		Bus(bus).register(#:(msg) {
			this.post(msg);
		});
	}
};

Bus.split => () {
	let _args = arguments;
	this.register(#:(msg) {
		for bus in _args {
			Bus(bus).post(msg);
		}
	});
};
