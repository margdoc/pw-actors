#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>

#include "err.h"

#ifndef debug
#define debug false
#endif

#ifdef __GNUC__
    #define UNUSED __attribute__((unused))
#else
    #define UNUSED
#endif

#define check_if_error(output, prompt) do { \
    if ((err = output) != 0)                \
        syserr(err, prompt);                \
} while (false)

/*
 * Makra na funkcje z biblioteki memory.h
 * Sprawdzają czy poprawnie zaalokowano pamięć
 */
#define malloc_and_check(src, size) do { \
    src = malloc(size);                  \
    if (src == NULL)                     \
        syserr(-1, "malloc failed");     \
} while (false)

#define realloc_and_check(src, size) do { \
    src = realloc(src, size);             \
    if (src == NULL)                      \
        syserr(-1, "realloc failed");     \
} while (false)

/*
 * Makra na funkcje z biblioteki pthreads.h
 * Sprawdzają czy poprawnie wykonano operacje
 */


/*
 * Muteksy i zmienne warunkowe
 */

#define mutex_init(lock) \
    check_if_error(pthread_mutex_init(lock, 0), "mutex init failed")

#define cond_init(cond) \
    check_if_error(pthread_cond_init(cond, 0), "cond init failed")

#define mutex_destroy(lock) \
    check_if_error(pthread_mutex_destroy(lock), "mutex destroy failed")

#define cond_destroy(cond) \
    check_if_error(pthread_cond_destroy(cond), "cond destroy failed")

#define mutex_lock(lock) \
    check_if_error(pthread_mutex_lock(lock), "mutex lock failed")

#define mutex_unlock(lock) \
    check_if_error(pthread_mutex_unlock(lock), "mutex unlock failed")

#define cond_wait(cond, lock) \
    check_if_error(pthread_cond_wait(cond, lock), "cond wait failed")

#define cond_signal(cond) \
    check_if_error(pthread_cond_signal(cond), "cond signal failed")

#define cond_broadcast(cond) \
    check_if_error(pthread_cond_broadcast(cond), "cond broadcast failed")


/*
 * Wątki
 */

#define thread_attr_init(state) \
    check_if_error(pthread_attr_init(&attr), "attr init failed"); \
    check_if_error(pthread_attr_setdetachstate(&attr, state), "attr setdetachstate failed")

#define thread_attr_destroy \
    check_if_error(pthread_attr_destroy(&attr), "attr destroy failed")

#define thread_create(tid, func) \
    check_if_error(pthread_create(tid, &attr, func, 0), "pthread create failed")

#define thread_join(tid) \
    check_if_error(pthread_join(tid, &retval), "pthread join failed")

#define thread_key_create(key) \
    check_if_error(pthread_key_create(key, NULL), "pthread key create failed")

#define thread_key_delete(key) \
    check_if_error(pthread_key_delete(key), "pthread key delete failed")\



/*
 * Semafory dla pisarzy i czytelników
 */

#define rwlock_init(lock) \
    check_if_error(pthread_rwlock_init(lock, 0), "rwlock init failed")

#define rwlock_destroy(lock) \
    check_if_error(pthread_rwlock_destroy(lock), "rwlock destroy failed")

#define rwlock_rdlock(lock) \
    check_if_error(pthread_rwlock_rdlock(lock), "rwlock rdlock failed")

#define rwlock_wrlock(lock) \
    check_if_error(pthread_rwlock_wrlock(lock), "rwlock wrlock failed")

#define rwlock_unlock(lock) \
    check_if_error(pthread_rwlock_unlock(lock), "rwlock unlock failed")



/*
 * Makra na obsługiwanie struktur współbieżnych
 */

#define entity_lock(entity) mutex_lock(&entity->lock)
#define entity_unlock(entity) mutex_unlock(&entity->lock)
#define entity_reader_lock(entity) rwlock_rdlock(&entity->rwlock)
#define entity_writer_lock(entity) rwlock_wrlock(&entity->rwlock)
#define entity_rw_unlock(entity) rwlock_unlock(&entity->rwlock)

#endif //UTILS_H
