/* Copyright (c) 2018, 2019, 2020 Dennis Wölfing
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

/* kernel/src/thread.cpp
 * Thread class.
 */

#include <assert.h>
#include <string.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/registers.h>

Thread* Thread::_current;
Thread* Thread::idleThread;
static Thread* firstThread;

__fpu_t initFpu;

Thread::Thread(Process* process) {
    contextChanged = false;
    interruptContext = nullptr;
    kernelStack = 0;
    next = nullptr;
    pendingSignals = nullptr;
    prev = nullptr;
    this->process = process;
    returnSignalMask = 0;
    signalMask = 0;
    signalMutex = KTHREAD_MUTEX_INITIALIZER;
    signalCond = KTHREAD_COND_INITIALIZER;
}

Thread::~Thread() {
    kernelSpace->unmapMemory(kernelStack, PAGESIZE);
}

void Thread::initializeIdleThread() {
    Process* idleProcess = xnew Process();
    idleProcess->addressSpace = kernelSpace;
    Process::addProcess(idleProcess);
    assert(idleProcess->pid == 0);
    idleThread = &idleProcess->mainThread;
    _current = idleThread;
}

void Thread::addThread(Thread* thread) {
    Interrupts::disable();
    thread->next = firstThread;
    if (firstThread) {
        firstThread->prev = thread;
    }
    firstThread = thread;
    Interrupts::enable();
}

void Thread::removeThread(Thread* thread) {
    if (thread->prev) {
        thread->prev->next = thread->next;
    } else {
        firstThread = thread->next;
    }

    if (thread->next) {
        thread->next->prev = thread->prev;
    }
}

InterruptContext* Thread::schedule(InterruptContext* context) {
    if (likely(!_current->contextChanged)) {
        _current->interruptContext = context;
        Registers::saveFpu(&_current->fpuEnv);
    } else {
        _current->contextChanged = false;
    }

    if (_current->next) {
        _current = _current->next;
    } else {
        if (firstThread) {
            _current = firstThread;
        } else {
            _current = idleThread;
        }
    }

    setKernelStack(_current->kernelStack + PAGESIZE);
    Registers::restoreFpu(&_current->fpuEnv);

    _current->process->addressSpace->activate();
    _current->checkSigalarm(true);
    _current->updatePendingSignals();
    return _current->interruptContext;
}

void Thread::updateContext(vaddr_t newKernelStack, InterruptContext* newContext,
            const __fpu_t* newFpuEnv) {
    Interrupts::disable();
    if (this == _current) {
        contextChanged = true;
    }

    if (kernelStack) {
        // TODO: We cannot unmap kernelStack on exec because it will remain in
        // use until scheduling. This is leaking 4 KiB on every execve(2).
    }
    kernelStack = newKernelStack;
    interruptContext = newContext;
    memcpy(fpuEnv, newFpuEnv, sizeof(__fpu_t));
    Interrupts::enable();
}
