#include <libfam/format.H>
#include <libfam/lock.H>

#define PTHREAD_PROCESS_SHARED 0 /* Not used */

typedef u32 pthread_key_t;
typedef i32 pthread_mutexattr_t;
typedef u32 pthread_mutex_t;
typedef u32 pthread_t;
typedef u32 pthread_cond_t;

void *pthread_getspecific(pthread_key_t key);
int pthread_key_delete(pthread_key_t key);
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_setspecific(pthread_key_t key, const void *value);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
LockGuard pthread_mutex_lock_guard(pthread_mutex_t *mutex);
pthread_t pthread_self(void);

