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

/* libc/src/thread/pthread_cond_signal.c
 * Signal a condition variable. (POSIX2008, called from C11)
 */

#define sched_yield __sched_yield
#include "thread.h"

int __cond_signal(__cond_t* cond) {
    while (__atomic_test_and_set(&cond->__state, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }

    if (cond->__first) {
        struct __cond_waiter* waiter = cond->__first;
        cond->__first = waiter->__next;
        __atomic_store_n(&waiter->__blocked, 0, __ATOMIC_RELEASE);
    }
    if (cond->__first) {
        cond->__first->__prev = NULL;
    } else {
        cond->__last = NULL;
    }

    __atomic_clear(&cond->__state, __ATOMIC_RELEASE);
    return 0;
}
__weak_alias(__cond_signal, pthread_cond_signal);
