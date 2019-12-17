#include "locks.h"
#include "error.h"

pthread_mutex_t busses_mutex;
pthread_mutex_t threads_mutex;
pthread_mutex_t output_mutex;

void init_locks(void) {
	if (pthread_mutex_init(&busses_mutex, NULL)) {
		error_general("Could not initialize global bus mutex!");
		return;
	}
	if (pthread_mutex_init(&threads_mutex, NULL)) {
		error_general("Could not initialize global threads mutex!");
		return;
	}
	if (pthread_mutex_init(&output_mutex, NULL)) {
		error_general("Could not initialize global output mutex!");
		return;
	}
}

void destroy_locks(void) {
	pthread_mutex_destroy(&busses_mutex);
	pthread_mutex_destroy(&threads_mutex);
	pthread_mutex_destroy(&output_mutex);
}
