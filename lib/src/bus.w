/**
 * MessageBus library for WendyScript, the only way
 *   to do parallel processing.
 * 
 * This high level interface is support underneath by
 *   the VM, as it needs to handle thread locking
 *   procedures.
 */

struct Bus => (name) [register, post, aggregate];

Bus.register => (fn /* accepting types */) {
    let register => (name, fn, args) native bus_register;
    ret register(this.name, fn, arguments);
};

Bus.post => (msg) {
    let post => (name, msg) native bus_post;
    ret post(this.name, msg);
};

Bus.aggregate => () {
	let post => (name, msg) native bus_post;
	let register => (name, fn, args) native bus_register;
	for bus in arguments {
		Bus(bus).register(#:(msg) {

		});
	}
};
