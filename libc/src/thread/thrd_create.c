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

/* libc/src/thread/thrd_create.c
 * Create a thread. (C11)
 */

#define sched_yield __sched_yield
#include "thread.h"
#include <stdnoreturn.h>

static noreturn void wrapperFunc(thrd_start_t func, void* arg) {
    while (__thread_self()->uthread.tid == -1) {
        sched_yield();
    }
    thrd_exit(func(arg));
}

int thrd_create(thrd_t* thread, thrd_start_t func, void* arg) {
    return threadWrapper(__thread_create(thread, NULL, wrapperFunc, func, arg));
}
