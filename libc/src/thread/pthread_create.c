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

/* libc/src/thread/pthread_create.c
 * Create a thread. (POSIX2008, called from C11)
 */

#define mmap __mmap
#define munmap __munmap
#define pthread_exit __pthread_exit
#define regfork __regfork
#define sched_yield __sched_yield
#include "thread.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define ALIGNUP(val, align) ((((val) - 1) & ~((align) - 1)) + (align))

static void prepareRegisters(regfork_t* registers, void* wrapper, void* func,
        void* arg, void* stack, size_t stackSize, void* tlsbase) {
    void* stackTop = (char*) stack + stackSize;

#ifdef __i386__
    uintptr_t* stackPointer = (uintptr_t*) stackTop;
    stackPointer -= 2;
    *--stackPointer = (uintptr_t) arg;
    *--stackPointer = (uintptr_t) func;
    stackPointer--;

    registers->__eax = 0;
    registers->__ebx = 0;
    registers->__ecx = 0;
    registers->__edx = 0;
    registers->__esi = 0;
    registers->__edi = 0;
    registers->__ebp = 0;
    registers->__eip = (uintptr_t) wrapper;
    registers->__eflags = 0;
    registers->__esp = (uintptr_t) stackPointer;
    registers->__tlsbase = (uintptr_t) tlsbase;
#elif defined(__x86_64__)
    registers->__rax = 0;
    registers->__rbx = 0;
    registers->__rcx = 0;
    registers->__rdx = 0;
    registers->__rsi = (uintptr_t) arg;
    registers->__rdi = (uintptr_t) func;
    registers->__rbp = 0;
    registers->__r8 = 0;
    registers->__r9 = 0;
    registers->__r10 = 0;
    registers->__r11 = 0;
    registers->__r12 = 0;
    registers->__r13 = 0;
    registers->__r14 = 0;
    registers->__r15 = 0;
    registers->__rip = (uintptr_t) wrapper;
    registers->__rflags = 0;
    registers->__rsp = (uintptr_t) stackTop - 8;
    registers->__tlsbase = (uintptr_t) tlsbase;
#else
#  error "pthread_create is unimplemented for this architecture."
#endif
}

#define STACK_SIZE (128 * 1024)

int __thread_create(__thread_t* restrict thread,
        const __thread_attr_t* restrict attr, void* restrict wrapper,
        void* restrict func, void* restrict arg) {
    (void) attr;

    __thread_t self = __thread_self();
    size_t tlsSize = ALIGNUP(self->uthread.tlsSize,
            alignof(struct __threadStruct));
    size_t mappingSize = ALIGNUP(tlsSize + sizeof(struct __threadStruct),
            PAGESIZE);

    void* tlsCopy = mmap(NULL, mappingSize, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!tlsCopy) return ENOMEM;

    void* stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!stack) {
        munmap(tlsCopy, mappingSize);
        return ENOMEM;
    }

    memcpy(tlsCopy, self->uthread.tlsMaster, self->uthread.tlsSize);
    __thread_t thr = (__thread_t) ((char*) tlsCopy + tlsSize);
    thr->uthread = self->uthread;
    thr->uthread.self = &thr->uthread;
    thr->uthread.tlsCopy = tlsCopy;
    thr->uthread.stack = stack;
    thr->uthread.stackSize = STACK_SIZE;
    thr->uthread.tid = -1;
    thr->prev = NULL;
    thr->mappingSize = mappingSize;
    thr->state = PREPARING;
    memset(thr->keyValues, 0, sizeof(thr->keyValues));

    regfork_t registers;
    prepareRegisters(&registers, wrapper, func, arg, stack, STACK_SIZE, thr);

    pid_t tid = regfork(RFTHREAD | RFMEM, &registers);
    if (tid < 0) {
        munmap(tlsCopy, mappingSize);
        munmap(stack, STACK_SIZE);
        return errno;
    }

    __mutex_lock(&__threadListMutex);
    thr->next = __threadList;
    __threadList->prev = thr;
    __threadList = thr;
    __mutex_unlock(&__threadListMutex);

    thr->uthread.tid = tid;
    __atomic_store_n(&thr->state, JOINABLE, __ATOMIC_RELEASE);
    *thread = thr;
    return 0;
}

static noreturn void wrapperFunc(void* (*func)(void*), void* arg) {
    __thread_t self = __thread_self();
    while (__atomic_load_n(&self->state, __ATOMIC_ACQUIRE) == PREPARING) {
        sched_yield();
    }
    pthread_exit(func(arg));
}

int __pthread_create(pthread_t* restrict thread,
        const pthread_attr_t* restrict attr, void* (*func)(void*),
        void* restrict arg) {
    return __thread_create(thread, attr, wrapperFunc, func, arg);
}
__weak_alias(__pthread_create, pthread_create);
