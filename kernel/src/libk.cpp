/* Copyright (c) 2016, 2017 Dennis Wölfing
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

/* kernel/src/libk.cpp
 * Functions used by libk.
 */

#include <assert.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/log.h>

static kthread_mutex_t heapLock = KTHREAD_MUTEX_INITIALIZER;

extern "C" void __assertionFailure(const char* assertion, const char* file,
        unsigned int line, const char* func) {
    Log::printf("Assertion failed: '%s' at function %s (%s:%u)\n",
            assertion, func, file, line);
    // Halt the kernel.
    while (true) asm volatile("cli; hlt");
}

extern "C" void __lockHeap(void) {
    kthread_mutex_lock(&heapLock);
}

extern "C" void* __mapMemory(size_t size) {
    return (void*) kernelSpace->mapMemory(size, PROT_READ | PROT_WRITE);
}

extern "C" void __unlockHeap(void) {
    kthread_mutex_unlock(&heapLock);
}

extern "C" void __unmapMemory(void* addr, size_t size) {
    kernelSpace->unmapMemory((vaddr_t) addr, size);
}
