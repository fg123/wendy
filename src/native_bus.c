#include "data.h"
#include "native.h"
#include "error.h"

#include <pthread.h>

/* struct data_block {

};

struct accepting_function {

};*/

struct bus {
    char* name;

    struct bus *next;
};

static pthread_mutex_t busses_mutex;
static struct bus * busses = 0;

double native_to_numeric(struct vm* vm, struct data* t);
char* native_to_string(struct vm* vm, struct data* t);

void native_bus_init(void) {
	if (pthread_mutex_init(&busses_mutex, NULL)) {
		error_general("Could not initialize global bus mutex!");
		return;
	}
}

void native_bus_destroy(void) {
	pthread_mutex_lock(&busses_mutex);
	while (busses) {
		safe_free(busses->name);
		struct bus * next = busses->next;
		safe_free(busses);
		busses = next;
	}
	pthread_mutex_destroy(&busses_mutex);
}

struct data native_bus_register(struct vm* vm, struct data* args) {
	// args = name, fn, accepting types (as a list)
	char* name = native_to_string(vm, &args[0]);
	if (args[1].type != D_FUNCTION) {
		error_runtime(vm->memory, vm->line, "Second argument to register must be a function!");
		return noneret_data();
	}
	printf("Bus Register!\n");
	pthread_mutex_lock(&busses_mutex);
	struct bus * curr = busses;
	while (curr) {
		printf("Testing %s with %s\n", curr->name, name);
		if (streq(curr->name, name)) break;
		curr = curr->next;
	}
	if (curr == 0) {
		printf("Not found, creating bus!\n");
		struct bus * new_bus = safe_malloc(sizeof(*new_bus));
		new_bus->name = safe_strdup(name);
		new_bus->next = busses;
		busses = new_bus;
		curr = new_bus;
	}
	else {
		printf("Found, no need to create\n");
	}
	pthread_mutex_unlock(&busses_mutex);
	return noneret_data();
}

struct data native_bus_post(struct vm* vm, struct data* args) {
	// args = name, message
	UNUSED(vm);
	UNUSED(args);
	return noneret_data();
}
