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

/* libc/include/threads.h
 * Threads.
 */

#ifndef _THREADS_H
#define _THREADS_H

#include <time.h>
#include <bits/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define thread_local _Thread_local

typedef __mutex_t mtx_t;
typedef __thread_t thrd_t;
typedef int (*thrd_start_t)(void *);

enum {
    thrd_success,
    thrd_busy,
    thrd_error,
    thrd_nomem,
    thrd_timedout
};

int mtx_lock(mtx_t*);
int mtx_unlock(mtx_t*);
int thrd_create(thrd_t*, thrd_start_t, void*);
thrd_t thrd_current(void);
__noreturn void thrd_exit(int);
int thrd_join(thrd_t, int*);

#ifdef __cplusplus
}
#endif

#endif
