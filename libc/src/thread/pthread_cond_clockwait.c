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

/* libc/src/thread/pthread_cond_clockwait.c
 * Wait on a condition variable for a given time. (called from C11)
 */

#define clock_gettime __clock_gettime
#define sched_yield __sched_yield
#include "thread.h"
#include <stdbool.h>

static bool timespecLess(struct timespec ts1, struct timespec ts2) {
    if (ts1.tv_sec < ts2.tv_sec) return true;
    if (ts1.tv_sec > ts2.tv_sec) return false;
    return ts1.tv_nsec < ts2.tv_nsec;
}

int __cond_clockwait(__cond_t* restrict cond, __mutex_t* restrict mutex,
        clockid_t clock, const struct timespec* restrict abstime) {
    if (abstime) {
        if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) {
            return EINVAL;
        }
        if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) {
            return EINVAL;
        }
    }

    while (__atomic_test_and_set(&cond->__state, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }

    __mutex_unlock(mutex);

    struct __cond_waiter waiter;
    waiter.__prev = cond->__last;
    waiter.__next = NULL;
    waiter.__blocked = 1;
    if (cond->__last) {
        cond->__last->__next = &waiter;
    } else {
        cond->__first = &waiter;
    }
    cond->__last = &waiter;

    __atomic_clear(&cond->__state, __ATOMIC_RELEASE);

    int result = 0;
    while (__atomic_load_n(&waiter.__blocked, __ATOMIC_ACQUIRE)) {
        if (abstime) {
            struct timespec now;
            clock_gettime(clock, &now);

            if (!timespecLess(now, *abstime)) {
                result = ETIMEDOUT;
                break;
            }
        }

        sched_yield();
    }

    if (result) {
        while (__atomic_test_and_set(&cond->__state, __ATOMIC_ACQUIRE)) {
            sched_yield();
        }

        // Only remove the waiter from the list if we were not unblocked
        // concurrently. In that case the waiter was already removed.
        if (__atomic_load_n(&waiter.__blocked, __ATOMIC_RELAXED)) {
            if (waiter.__prev) {
                waiter.__prev->__next = waiter.__next;
            } else {
                cond->__first = waiter.__next;
            }
            if (waiter.__next) {
                waiter.__next->__prev = waiter.__prev;
            } else {
                cond->__last = waiter.__prev;
            }
        }

        __atomic_clear(&cond->__state, __ATOMIC_RELEASE);
    }

    __mutex_lock(mutex);
    return result;
}
__weak_alias(__cond_clockwait, pthread_cond_clockwait);
