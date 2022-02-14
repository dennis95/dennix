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

/* libc/src/thread/pthread_mutex_clocklock.c
 * Try to lock a mutex within a given time. (called from C11)
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

int __mutex_clocklock(__mutex_t* restrict mutex, clockid_t clock,
        const struct timespec* restrict abstime) {
    while (true) {
        int result = __mutex_trylock(mutex);
        if (result != EBUSY) return result;

        if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) {
            return EINVAL;
        }
        struct timespec now;
        if (clock_gettime(clock, &now) != 0) return EINVAL;
        if (!timespecLess(now, *abstime)) {
            return ETIMEDOUT;
        }

        sched_yield();
    }
}
__weak_alias(__mutex_clocklock, pthread_mutex_clocklock);
