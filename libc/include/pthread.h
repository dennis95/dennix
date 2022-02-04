/* Copyright (c) 2022 Dennis Wölfing
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

#define PTHREAD_MUTEX_INITIALIZER _MUTEX_INIT(_MUTEX_NORMAL)

int pthread_create(pthread_t* __restrict, const pthread_attr_t* __restrict,
        void* (*)(void*), void* __restrict);
__noreturn void pthread_exit(void*);
int pthread_join(pthread_t, void**);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);
pthread_t pthread_self(void);

#ifdef __cplusplus
}
#endif

#endif
