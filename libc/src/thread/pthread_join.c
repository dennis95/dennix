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

/* libc/src/thread/pthread_join.c
 * Wait for thread termination. (POSIX2008, called from C11)
 */

#define munmap __munmap
#define sched_yield __sched_yield
#include "thread.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>

int __thread_join(__thread_t thread, union ThreadResult* result) {
    char expected = EXITED;
    while (!__atomic_compare_exchange_n(&thread->state, &expected, JOINED,
            false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        if (expected == DETACHED) {
            return EINVAL;
        } else if (expected != JOINABLE) {
            abort();
        }
        sched_yield();
        expected = EXITED;
    }

    *result = thread->result;
    munmap(thread->uthread.tlsCopy, thread->mappingSize);
    return 0;
}

int __pthread_join(pthread_t thread, void** result) {
    union ThreadResult threadResult;
    int ret = __thread_join(thread, &threadResult);
    if (result) *result = threadResult.p;
    return ret;
}
__weak_alias(__pthread_join, pthread_join);
