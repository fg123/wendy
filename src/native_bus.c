#include "data.h"
#include "native.h"

struct bus {
    char* name;
    
    struct bus *next;
};

static struct bus * busses = 0;

struct data native_bus_register(struct vm* vm, struct data* args) {
    
}

struct data native_bus_post(struct vm* vm, struct data* args);