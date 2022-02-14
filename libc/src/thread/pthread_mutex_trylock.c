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

/* libc/src/thread/pthread_mutex_trylock.c
 * Try to lock a mutex. (POSIX2008, called from C89)
 */

#define sched_yield __sched_yield
#include "thread.h"
#include <stdbool.h>
#include <stdint.h>

int __mutex_trylock(__mutex_t* mutex) {
    if (mutex->__type == _MUTEX_NORMAL) {
        if (__atomic_test_and_set(&mutex->__state, __ATOMIC_ACQUIRE)) {
            return EBUSY;
        }
        return 0;
    } else if (mutex->__type == _MUTEX_RECURSIVE) {
        pid_t tid = __thread_self()->uthread.tid;

        while (true) {
            int expected = UNLOCKED;
            if (__atomic_compare_exchange_n(&mutex->__state, &expected, BUSY,
                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                mutex->__owner = tid;
                mutex->__count = 1;
                __atomic_store_n(&mutex->__state, LOCKED, __ATOMIC_RELEASE);
                return 0;
            }

            expected = LOCKED;
            if (__atomic_compare_exchange_n(&mutex->__state, &expected, BUSY,
                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                if (mutex->__owner == tid) {
                    if (mutex->__count == SIZE_MAX) {
                        __atomic_store_n(&mutex->__state, LOCKED,
                                __ATOMIC_RELAXED);
                        return EAGAIN;
                    }
                    mutex->__count++;
                    __atomic_store_n(&mutex->__state, LOCKED, __ATOMIC_RELEASE);
                    return 0;
                }
                __atomic_store_n(&mutex->__state, LOCKED, __ATOMIC_RELAXED);
                return EBUSY;
            }

            sched_yield();
        }
    }

    return EINVAL;
}
__weak_alias(__mutex_trylock, pthread_mutex_trylock);
