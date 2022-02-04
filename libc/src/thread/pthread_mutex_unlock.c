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

/* libc/src/thread/pthread_mutex_unlock.c
 * Unlock a mutex. (POSIX2008, called from C89)
 */

#define sched_yield __sched_yield
#include "thread.h"
#include <errno.h>
#include <stdbool.h>

int __mutex_unlock(__mutex_t* mutex) {
    if (mutex->__type == _MUTEX_NORMAL) {
        __atomic_clear(&mutex->__state, __ATOMIC_RELEASE);
        return 0;
    } else if (mutex->__type == _MUTEX_RECURSIVE) {
        int expected = LOCKED;
        while (!__atomic_compare_exchange_n(&mutex->__state, &expected, BUSY,
                false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            if (expected == UNLOCKED) {
                return EPERM;
            }
            sched_yield();
            expected = LOCKED;
        }

        pid_t tid = __thread_self()->uthread.tid;
        if (mutex->__owner != tid) {
            __atomic_store_n(&mutex->__state, LOCKED, __ATOMIC_RELAXED);
            return EPERM;
        }

        mutex->__count--;
        if (mutex->__count == 0) {
            mutex->__owner = -1;
            __atomic_store_n(&mutex->__state, UNLOCKED, __ATOMIC_RELEASE);
            return 0;
        }
        __atomic_store_n(&mutex->__state, LOCKED, __ATOMIC_RELEASE);
        return 0;
    }

    return EINVAL;
}
__weak_alias(__mutex_unlock, pthread_mutex_unlock);
