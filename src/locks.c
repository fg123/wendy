#include "locks.h"
#include "error.h"

pthread_mutex_t busses_mutex;
pthread_mutex_t threads_mutex;
pthread_mutex_t output_mutex;
pthread_mutex_t malloc_mutex;

void init_locks(void) {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&busses_mutex, NULL)) {
		error_general("Could not initialize global bus mutex!");
		return;
	}
	if (pthread_mutex_init(&threads_mutex, NULL)) {
		error_general("Could not initialize global threads mutex!");
		return;
	}
	if (pthread_mutex_init(&output_mutex, &attr)) {
		error_general("Could not initialize global output mutex!");
		return;
	}
	if (pthread_mutex_init(&malloc_mutex, NULL)) {
		error_general("Could not initialize global malloc mutex!");
		return;
	}
	pthread_mutexattr_destroy(&attr);
}

void destroy_locks(void) {
	pthread_mutex_destroy(&busses_mutex);
	pthread_mutex_destroy(&threads_mutex);
	pthread_mutex_destroy(&output_mutex);
	pthread_mutex_destroy(&malloc_mutex);
}

void safe_lock_impl(pthread_mutex_t *mutex) {
	if (pthread_mutex_lock(mutex)) {
		error_general("Could not lock mutex!");
		safe_exit(2);
	}
}

void safe_unlock_impl(pthread_mutex_t *mutex) {
	if (pthread_mutex_unlock(mutex)) {
		error_general("Could not unlock mutex!");
		safe_exit(2);
	}
}
