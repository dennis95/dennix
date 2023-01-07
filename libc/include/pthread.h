/* Copyright (c) 2022, 2023 Dennis Wölfing
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* libc/include/pthread.h
 * Threads.
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <sched.h>
#include <time.h>
#include <bits/pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PTHREAD_MUTEX_DEFAULT _MUTEX_NORMAL
#define PTHREAD_MUTEX_NORMAL _MUTEX_NORMAL
#define PTHREAD_MUTEX_RECURSIVE _MUTEX_RECURSIVE

#define PTHREAD_COND_INITIALIZER _COND_INIT
#define PTHREAD_MUTEX_INITIALIZER _MUTEX_INIT(_MUTEX_NORMAL)
#define PTHREAD_ONCE_INIT _ONCE_INIT

#define PTHREAD_NULL ((pthread_t) 0)

int pthread_cond_broadcast(pthread_cond_t*);
int pthread_cond_clockwait(pthread_cond_t* __restrict,
        pthread_mutex_t* __restrict, clockid_t,
        const struct timespec* __restrict);
int pthread_cond_destroy(pthread_cond_t*);
int pthread_cond_init(pthread_cond_t* __restrict,
        const pthread_condattr_t* __restrict);
int pthread_cond_signal(pthread_cond_t*);
int pthread_cond_timedwait(pthread_cond_t* __restrict,
        pthread_mutex_t* __restrict, const struct timespec* __restrict);
int pthread_cond_wait(pthread_cond_t* __restrict, pthread_mutex_t* __restrict);
int pthread_condattr_destroy(pthread_condattr_t*);
int pthread_condattr_getclock(const pthread_condattr_t* __restrict,
        clockid_t* __restrict);
int pthread_condattr_init(pthread_condattr_t*);
int pthread_condattr_setclock(pthread_condattr_t*, clockid_t);
int pthread_create(pthread_t* __restrict, const pthread_attr_t* __restrict,
        void* (*)(void*), void* __restrict);
int pthread_detach(pthread_t);
int pthread_equal(pthread_t, pthread_t);
__noreturn void pthread_exit(void*);
void* pthread_getspecific(pthread_key_t key);
int pthread_join(pthread_t, void**);
int pthread_key_create(pthread_key_t*, void (*)(void*));
int pthread_key_delete(pthread_key_t);
int pthread_mutex_clocklock(pthread_mutex_t* __restrict, clockid_t,
        const struct timespec* __restrict);
int pthread_mutex_destroy(pthread_mutex_t*);
int pthread_mutex_init(pthread_mutex_t* __restrict,
        const pthread_mutexattr_t* __restrict);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_timedlock(pthread_mutex_t* __restrict,
        const struct timespec* __restrict);
int pthread_mutex_trylock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);
int pthread_mutexattr_destroy(pthread_mutexattr_t*);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* __restrict,
        int* __restrict);
int pthread_mutexattr_init(pthread_mutexattr_t*);
int pthread_mutexattr_settype(pthread_mutexattr_t*, int);
int pthread_once(pthread_once_t*, void (*)(void));
pthread_t pthread_self(void);
int pthread_setspecific(pthread_key_t key, const void* value);

#ifdef __cplusplus
}
#endif

#endif
