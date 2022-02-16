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

/* libc/include/bits/thread.h
 * Threads.
 */

#ifndef _BITS_THREAD_H
#define _BITS_THREAD_H

#include <dennix/types.h>

typedef struct __threadStruct* __thread_t;

typedef int __thread_attr_t;

struct __cond_waiter {
    struct __cond_waiter* __prev;
    struct __cond_waiter* __next;
    char __blocked;
};

typedef struct {
    struct __cond_waiter* __first;
    struct __cond_waiter* __last;
    __clockid_t __clock;
    char __state;
} __cond_t;

typedef struct {
    char __type;
    char __state;
    __pid_t __owner;
    __SIZE_TYPE__ __count;
} __mutex_t;

#define _MUTEX_NORMAL 0
#define _MUTEX_RECURSIVE 1

#define _COND_INIT { (void*) 0, (void*) 0, CLOCK_REALTIME, 0 }
#define _MUTEX_INIT(type) { (type), 0, -1, 0 }

#endif
