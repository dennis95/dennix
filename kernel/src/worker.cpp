/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/src/worker.cpp
 * Kernel worker thread.
 */

#include <sched.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/thread.h>
#include <dennix/kernel/worker.h>

static WorkerJob* firstJob;
static WorkerJob* lastJob;

static NORETURN void worker(void) {
    while (true) {
        Interrupts::disable();
        WorkerJob* job = firstJob;
        firstJob = nullptr;
        Interrupts::enable();

        if (!job) {
            sched_yield();
        }

        while (job) {
            WorkerJob* next = job->next;
            job->func(job->context);
            job = next;
        }
    }
}

void WorkerThread::addJob(WorkerJob* job) {
    // This function needs to be called with interrupts disabled.

    job->next = nullptr;
    if (!firstJob) {
        firstJob = job;
        lastJob = job;
    } else {
        lastJob->next = job;
        lastJob = job;
    }
}

void WorkerThread::initialize() {
    Thread* thread = xnew Thread(Thread::idleThread->process);
    vaddr_t stack = kernelSpace->mapMemory(PAGESIZE, PROT_READ | PROT_WRITE);
    if (!stack) PANIC("Failed to allocate stack for worker thread");
    InterruptContext* context = (InterruptContext*)
            (stack + PAGESIZE - sizeof(InterruptContext));
    *context = {};

#ifdef __i386__
    context->eip = (vaddr_t) worker;
    context->cs = 0x8;
    context->eflags = 0x200;
    context->esp = stack + PAGESIZE - sizeof(void*);
    context->ss = 0x10;
#elif defined(__x86_64__)
    context->rip = (vaddr_t) worker;
    context->cs = 0x8;
    context->rflags = 0x200;
    context->rsp = stack + PAGESIZE - sizeof(void*);
    context->ss = 0x10;
#else
#  error "InterruptContext in WorkerThread is uninitialized."
#endif

    thread->updateContext(stack, context, &initFpu);
    Thread::addThread(thread);
}
