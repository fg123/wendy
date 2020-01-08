#ifndef LOCKS_H
#define LOCKS_H

#include <pthread.h>

extern pthread_mutex_t busses_mutex;
extern pthread_mutex_t threads_mutex;
extern pthread_mutex_t output_mutex;
extern pthread_mutex_t malloc_mutex;

void init_locks(void);
void destroy_locks(void);

#define safe_lock(m) safe_lock_impl(m)
#define safe_unlock(m) safe_unlock_impl(m)

void safe_lock_impl(pthread_mutex_t *mutex);
void safe_unlock_impl(pthread_mutex_t *mutex);

#endif
