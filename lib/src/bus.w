/**
 * MessageBus library for WendyScript, the only way
 *   to do parallel processing.
 * 
 * This high level interface is support underneath by
 *   the VM, as it needs to handle thread locking
 *   procedures.
 */


struct Bus => (name) [register, post];

Bus.register => (fn /* accepting types */) {
    let register => () native bus_register;
    ret register(this.name, fn, arguments);
};

Bus.post => (msg) {
    let post => () native bus_post;
    ret post(this.name, msg);
};
