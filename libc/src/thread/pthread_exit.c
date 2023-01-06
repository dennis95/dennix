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

/* libc/src/thread/pthread_exit.c
 * Exit the current thread. (POSIX2008, called from C11)
 */

#define munmap __munmap
#include "thread.h"
#include <stdbool.h>
#include <stdlib.h>
#include <dennix/exit.h>
#include <sys/mman.h>
#include <sys/syscall.h>

DEFINE_SYSCALL(SYSCALL_EXIT_THREAD, __noreturn void, exit_thread,
        (const struct exit_thread*));

__attribute__((weak))
void __key_run_destructors(void) {}

__noreturn void __thread_exit(union ThreadResult result) {
    __thread_t self = __thread_self();

    __key_run_destructors();

    __mutex_lock(&__threadListMutex);
    if (self->next) {
        self->next->prev = self->prev;
    }
    if (self->prev) {
        self->prev->next = self->next;
    } else {
        __threadList = self->next;
    }
    __mutex_unlock(&__threadListMutex);

    self->result = result;

    struct exit_thread data = {0};
    data.flags = 0;
    data.status = 0;
    data.unmapAddress = self->uthread.stack;
    data.unmapSize = self->uthread.stackSize;

    char state = JOINABLE;
    if (__atomic_compare_exchange_n(&self->state, &state, EXITED,
            false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
        // We can be joined by another thread. That thread will cleanup our TLS
        // copy.
    } else if (state == DETACHED) {
        munmap(self->uthread.tlsCopy, self->mappingSize);
    } else {
        // The thread state is inconsistent. It is not safe to continue.
        abort();
    }

    exit_thread(&data);
}

__noreturn void __pthread_exit(void* result) {
    union ThreadResult threadResult;
    threadResult.p = result;
    __thread_exit(threadResult);
}
__weak_alias(__pthread_exit, pthread_exit);
