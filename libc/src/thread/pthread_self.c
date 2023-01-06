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

/* libc/src/thread/pthread_self.c
 * Get the current thread id. (POSIX2008, called from C89)
 */

#include "thread.h"
#include <limits.h>

#define ALIGNUP(val, align) ((((val) - 1) & ~((align) - 1)) + (align))

__thread_t __threadList;
__mutex_t __threadListMutex = _MUTEX_INIT(_MUTEX_NORMAL);

void __initializeThreads(void) {
    __thread_t self = __thread_self();
    size_t uthreadOffset = ALIGNUP(self->uthread.tlsSize,
            alignof(struct __threadStruct));
    self->mappingSize = ALIGNUP(uthreadOffset + UTHREAD_SIZE, PAGESIZE);
    self->state = JOINABLE;
    __threadList = self;
}

__thread_t __thread_self(void) {
    __thread_t result;
#ifdef __i386__
    asm("mov %%gs:0, %0" : "=r"(result));
#elif defined(__x86_64__)
    asm("mov %%fs:0, %0" : "=r"(result));
#else
#  error "pthread_self is unimplemented for this architecture."
#endif
    return result;
}
__weak_alias(__thread_self, pthread_self);
__weak_alias(__thread_self, thrd_current);
