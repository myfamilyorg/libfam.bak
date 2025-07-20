#include <libfam/format.H>
#include <libfam/lock.H>

/* Typedefs (place in a header if separating declaration/definition) */
typedef unsigned int pthread_key_t;
typedef int pthread_mutexattr_t;
typedef u32 pthread_mutex_t;	/* Alias to your Lock (u32) */
typedef unsigned int pthread_t; /* For pthread_self */

/* TLS-related stubs (noops with MDB_NOTLS) */
void *pthread_getspecific(pthread_key_t key __attribute__((unused))) {
	panic("Unexpected call with MDB_NOTLS");
	return NULL;
}

int pthread_key_delete(pthread_key_t key __attribute__((unused))) {
	panic("Unexpected call with MDB_NOTLS");
	return -1;
}

int pthread_key_create(pthread_key_t *key __attribute__((unused)),
		       void (*destructor)(void *) __attribute__((unused))) {
	panic("Unexpected call with MDB_NOTLS");
	return -1;
}

int pthread_setspecific(pthread_key_t key __attribute__((unused)),
			const void *value __attribute__((unused))) {
	panic("Unexpected call with MDB_NOTLS");
	return -1;
}

/* Mutex attribute stubs (dummies; no real attrs in your Lock) */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr
			      __attribute__((unused))) {
	return 0; /* Success */
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr
				 __attribute__((unused)),
				 int pshared __attribute__((unused))) {
	return 0; /* Success (shared implicit via mmap) */
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
	if (attr) *attr = 0; /* Dummy init */
	return 0;	     /* Success */
}

/* Mutex operations (wrap your custom Lock; assume you have
 * lock_exclusive/unlock_exclusive as suggested) */
int pthread_mutex_unlock(pthread_mutex_t *mutex __attribute__((unused))) {
	LockGuardImpl lg __attribute__((unused)) = wlock(mutex);
	/*println("mutex_unlock");*/
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex __attribute__((unused))) {
	LockGuardImpl lg;
	/*println("mutex_lock");*/
	lg.lock = mutex;
	lg.is_write = true;
	lockguard_cleanup(&lg);
	return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr
		       __attribute__((unused))) {
	if (mutex) *mutex = LOCK_INIT; /* Your init value (0) */
	return 0;		       /* Success */
}

/* Misc stub */
pthread_t pthread_self(void) {
	return 0; /* Fixed stub; or implement via syscall if desired */
}

LockGuard pthread_mutex_lock_guard(pthread_mutex_t *mutex) {
	LockGuardImpl ret = wlock(mutex);
	/*println("mutex_lock_guard");*/
	return ret;
}
