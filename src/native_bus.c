#include "data.h"
#include "native.h"
#include "error.h"

#include <pthread.h>

struct accepting_function {
	struct function_entry entry;
	struct accepting_function* next;
};

struct bus {
    char* name;
	struct accepting_function *functions;
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
		while (busses->functions) {
			struct accepting_function* next = busses->functions->next;
			safe_free(busses->functions);
			busses->functions = next;
		}
		struct bus * next = busses->next;
		safe_free(busses);
		busses = next;
	}
	pthread_mutex_destroy(&busses_mutex);
}

static struct bus* find_or_create(char* name) {
	struct bus * curr = busses;
	while (curr) {
		if (streq(curr->name, name)) break;
		curr = curr->next;
	}
	if (curr == 0) {
		struct bus * new_bus = safe_malloc(sizeof(*new_bus));
		new_bus->name = safe_strdup(name);
		new_bus->functions = 0;
		new_bus->next = busses;
		busses = new_bus;
		curr = new_bus;
	}
	return curr;
}

struct data native_bus_register(struct vm* vm, struct data* args) {
	// args = name, fn, accepting types (as a list)
	char* name = native_to_string(vm, &args[0]);
	if (args[1].type != D_FUNCTION) {
		error_runtime(vm->memory, vm->line, "Argument to register must be a function!");
		return noneret_data();
	}
	pthread_mutex_lock(&busses_mutex);
	struct bus * curr = find_or_create(name);

	struct accepting_function* fn = safe_malloc(sizeof(*fn));
	fn->entry.bytecode = vm->bytecode;
	fn->entry.instruction_ptr = (address)args[1].value.reference[0].value.number;

	fn->next = curr->functions;
	curr->functions = fn;

	pthread_mutex_unlock(&busses_mutex);
	return noneret_data();
}

void* dispatch_run_vm (void* arg);

struct data native_bus_post(struct vm* vm, struct data* args) {
	// args = name, message
	char* name = native_to_string(vm, &args[0]);
	pthread_mutex_lock(&busses_mutex);
	struct bus * curr = find_or_create(name);
	struct accepting_function *fn = curr->functions;
	while (fn) {
		struct function_entry *entry = safe_malloc(sizeof(*entry));
		*entry = fn->entry;
		// TODO(felixguo): this will leak reference, will need to make it deep copy
		entry->arg = copy_data(args[1]);
		pthread_t thread;
		if (pthread_create(&thread, NULL, dispatch_run_vm, entry)) {
			error_runtime(vm->memory, vm->line, "Could not create pthread!");
		}
		fn = fn->next;
	}
	pthread_mutex_unlock(&busses_mutex);
	return noneret_data();
}
