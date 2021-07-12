#ifndef MODULE_H
#define MODULE_H

// Handles a compiled module 

struct module {
    uint8_t* bytecode;
    size_t length;

    size_t num_globals;
};

void module_write_to_file(struct* module)
#endif