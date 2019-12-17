#include "data.h"
#include "native.h"
#include "error.h"
#include "locks.h"

#include <pthread.h>

struct accepting_function {
	struct data function;
	struct accepting_function* next;
};

struct bus {
    char* name;
	struct accepting_function *functions;
    struct bus *next;
};

struct thread {
	pthread_t pthread;
	struct thread * next;
};

static struct bus * busses = 0;
static struct thread * threads = 0;

double native_to_numeric(struct vm* vm, struct data* t);
char* native_to_string(struct vm* vm, struct data* t);

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
	pthread_mutex_lock(&threads_mutex);
	while (threads) {
		pthread_join(threads->pthread, NULL);
		struct thread * next = threads->next;
		safe_free(threads);
		threads = next;
	}
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
	fn->function = copy_data(args[1]);

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
		entry->bytecode = vm->bytecode;
		entry->instruction_ptr = (address) fn->function.value.reference[0].value.number;
		entry->closure = copy_data(fn->function.value.reference[1]);

		// TODO(felixguo): this will leak reference, will need to make it deep copy
		entry->arg = copy_data(args[1]);

		pthread_mutex_lock(&threads_mutex);
		struct thread * thread = safe_malloc(sizeof(*thread));
		thread->next = threads;
		threads = thread;
		pthread_mutex_unlock(&threads_mutex);
		if (pthread_create(&thread->pthread, NULL, dispatch_run_vm, entry)) {
			error_runtime(vm->memory, vm->line, "Could not create pthread!");
		}
		fn = fn->next;
	}
	pthread_mutex_unlock(&busses_mutex);
	return noneret_data();
}
