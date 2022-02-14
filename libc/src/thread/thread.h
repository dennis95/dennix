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

/* libc/src/thread/thread.h
 * Internal thread definitions.
 */

#ifndef THREAD_H
#define THREAD_H

#include <errno.h>
#include <stdalign.h>
#include <threads.h>
#include <pthread.h>
#include <dennix/uthread.h>
#include <sys/types.h>

union ThreadResult {
    void* p;
    int i;
};

struct __threadStruct {
    struct uthread uthread;
    __mutex_t joinMutex;
    union ThreadResult result;
    size_t mappingSize;
};

_Static_assert(sizeof(struct __threadStruct) <= UTHREAD_SIZE,
        "struct __threadStruct is too large");
_Static_assert(alignof(struct __threadStruct) == alignof(struct uthread),
        "struct __threadStruct has wrong alignment requirement");

#define UNLOCKED 0
#define LOCKED 1
#define BUSY 2

static inline int threadWrapper(int error) {
    switch (error) {
    case 0: return thrd_success;
    case EBUSY: return thrd_busy;
    case ENOMEM: return thrd_nomem;
    case ETIMEDOUT: return thrd_timedout;
    default: return thrd_error;
    }
}

int __mutex_clocklock(__mutex_t* restrict mutex, clockid_t clock,
        const struct timespec* restrict abstime);
int __mutex_lock(__mutex_t* mutex);
int __mutex_trylock(__mutex_t* mutex);
int __mutex_unlock(__mutex_t* mutex);
int __thread_create(__thread_t* restrict thread,
        const __thread_attr_t* restrict attr, void* restrict wrapper,
        void* restrict func, void* restrict arg);
__noreturn void __thread_exit(union ThreadResult result);
int __thread_join(__thread_t thread, union ThreadResult* result);
__thread_t __thread_self(void);

#endif
