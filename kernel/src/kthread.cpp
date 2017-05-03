/* Copyright (c) 2017 Dennis Wölfing
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

/* kernel/src/kthread.cpp
 * Kernel threading functions.
 */

#include <sched.h>
#include <dennix/kernel/kthread.h>

int kthread_mutex_lock(kthread_mutex_t* mutex) {
    while (__atomic_test_and_set(mutex, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
    return 0;
}

int kthread_mutex_unlock(kthread_mutex_t* mutex) {
    __atomic_clear(mutex, __ATOMIC_RELEASE);
    return 0;
}
