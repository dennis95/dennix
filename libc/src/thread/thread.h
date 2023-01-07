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

/* libc/src/thread/thread.h
 * Internal thread definitions.
 */

#ifndef THREAD_H
#define THREAD_H

#include <errno.h>
#include <limits.h>
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
    __thread_t prev;
    __thread_t next;
    union ThreadResult result;
    size_t mappingSize;
    char state;
    void* keyValues[PTHREAD_KEYS_MAX];
};

extern __thread_t __threadList;
extern __mutex_t __threadListMutex;

_Static_assert(sizeof(struct __threadStruct) <= UTHREAD_SIZE,
        "struct __threadStruct is too large");
_Static_assert(alignof(struct __threadStruct) == alignof(struct uthread),
        "struct __threadStruct has wrong alignment requirement");

// Mutex states
#define UNLOCKED 0
#define LOCKED 1
#define BUSY 2

// Thread states
#define PREPARING 0
#define JOINABLE 1
#define EXITED 2
#define DETACHED 3
#define JOINED 4

static inline int threadWrapper(int error) {
    switch (error) {
    case 0: return thrd_success;
    case EBUSY: return thrd_busy;
    case ENOMEM: return thrd_nomem;
    case ETIMEDOUT: return thrd_timedout;
    default: return thrd_error;
    }
}

void __call_once(__once_t* once, void (*func)(void));
int __cond_broadcast(__cond_t* cond);
int __cond_clockwait(__cond_t* restrict cond, __mutex_t* restrict mutex,
        clockid_t clock, const struct timespec* restrict abstime);
int __cond_signal(__cond_t* cond);
int __key_create(__key_t* key, void (*destructor)(void*));
int __key_delete(__key_t key);
void* __key_getspecific(__key_t key);
void __key_run_destructors(void);
int __key_setspecific(__key_t key, const void* value);
int __mutex_clocklock(__mutex_t* restrict mutex, clockid_t clock,
        const struct timespec* restrict abstime);
int __mutex_lock(__mutex_t* mutex);
int __mutex_trylock(__mutex_t* mutex);
int __mutex_unlock(__mutex_t* mutex);
int __thread_create(__thread_t* restrict thread,
        const __thread_attr_t* restrict attr, void* restrict wrapper,
        void* restrict func, void* restrict arg);
int __thread_equal(__thread_t t1, __thread_t t2);
__noreturn void __thread_exit(union ThreadResult result);
int __thread_detach(__thread_t thread);
int __thread_join(__thread_t thread, union ThreadResult* result);
__thread_t __thread_self(void);

#endif
