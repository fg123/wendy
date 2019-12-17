#ifndef LOCKS_H
#define LOCKS_H

#include <pthread.h>

extern pthread_mutex_t busses_mutex;
extern pthread_mutex_t threads_mutex;
extern pthread_mutex_t output_mutex;
extern pthread_mutex_t malloc_mutex;

void init_locks(void);
void destroy_locks(void);
#endif
