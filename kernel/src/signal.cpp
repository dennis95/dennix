/* Copyright (c) 2017, 2018 Dennis Wölfing
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

/* kernel/src/signal.cpp
 * Signals.
 */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/signal.h>
#include <dennix/kernel/syscall.h>

struct SignalStackFrame {
#ifdef __i386__
    int signoParam;
    siginfo_t* infoParam;
    void* contextParam;
    siginfo_t siginfo;
    ucontext_t ucontext;
#else
#  error "SignalStackFrame is undefined for this architecture."
#endif
};

static sigset_t defaultIgnoredSignals = _SIGSET(SIGCHLD) | _SIGSET(SIGURG);

extern "C" {
volatile unsigned long signalPending = 0;
}

static inline bool isMoreImportantSignalThan(int signal1, int signal2) {
    if (signal1 == SIGKILL) return true;
    if (signal2 == SIGKILL) return false;
    if (signal1 == SIGSTOP) return true;
    if (signal2 == SIGSTOP) return false;
    return signal1 <= signal2;
}

extern "C" InterruptContext* handleSignal(InterruptContext* context) {
    return Thread::current()->handleSignal(context);
}

InterruptContext* Thread::handleSignal(InterruptContext* context) {
    kthread_mutex_lock(&signalMutex);
    assert(pendingSignals);
    assert(signalPending);

    // Choose the next unblocked pending signal.
    PendingSignal* pending;

    if (!sigismember(&signalMask, pendingSignals->siginfo.si_signo)) {
        pending = pendingSignals;
        pendingSignals = pending->next;
    } else {
        PendingSignal* currentSignal = pendingSignals;
        while (currentSignal->next && sigismember(&signalMask,
                currentSignal->next->siginfo.si_signo)) {
            currentSignal = currentSignal->next;
        }
        assert(currentSignal->next);

        pending = currentSignal->next;
        currentSignal->next = pending->next;
    }

    siginfo_t siginfo = pending->siginfo;
    delete pending;

    updatePendingSignals();
    kthread_mutex_unlock(&signalMutex);

    struct sigaction action = process->sigactions[siginfo.si_signo];
    assert(!(action.sa_handler == SIG_IGN || (action.sa_handler == SIG_DFL &&
            sigismember(&defaultIgnoredSignals, siginfo.si_signo))));

    if (action.sa_handler == SIG_DFL) {
        process->terminateBySignal(siginfo);
        sched_yield();
        __builtin_unreachable();
    }

    ucontext_t ucontext;
    ucontext.uc_link = nullptr;
    ucontext.uc_sigmask = signalMask;
    ucontext.uc_stack.ss_sp = nullptr;
    ucontext.uc_stack.ss_size = 0;
    ucontext.uc_stack.ss_flags = SS_DISABLE;

#ifdef __i386__
    ucontext.uc_mcontext.__eax = context->eax;
    ucontext.uc_mcontext.__ebx = context->ebx;
    ucontext.uc_mcontext.__ecx = context->ecx;
    ucontext.uc_mcontext.__edx = context->edx;
    ucontext.uc_mcontext.__esi = context->esi;
    ucontext.uc_mcontext.__edi = context->edi;
    ucontext.uc_mcontext.__ebp = context->ebp;
    ucontext.uc_mcontext.__eip = context->eip;
    ucontext.uc_mcontext.__eflags = context->eflags;
    ucontext.uc_mcontext.__esp = context->esp;

    uintptr_t frameAddress = (context->esp - sizeof(SignalStackFrame)) & ~0xF;
    SignalStackFrame* frame = (SignalStackFrame*) frameAddress;
    frame->signoParam = siginfo.si_signo;
    frame->infoParam = &frame->siginfo;
    frame->contextParam = &frame->ucontext;
    frame->siginfo = siginfo;
    frame->ucontext = ucontext;

    uintptr_t* sigreturnPointer = (uintptr_t*) frameAddress - 1;
    *sigreturnPointer = process->sigreturn;

    context->eip = (uintptr_t) action.sa_sigaction;
    context->esp = (uintptr_t) sigreturnPointer;
    context->eflags &= ~0x400; // Direction Flag
#else
#  error "Creation of SignalStackFrame is unimplemented for this architecture."
#endif

    signalMask |= action.sa_mask | _SIGSET(siginfo.si_signo);

    return context;
}

void Process::raiseSignal(siginfo_t siginfo) {
    mainThread.raiseSignal(siginfo);
}

void Thread::raiseSignal(siginfo_t siginfo) {
    AutoLock lock(&signalMutex);

    struct sigaction action = process->sigactions[siginfo.si_signo];

    if (action.sa_handler == SIG_IGN || (action.sa_handler == SIG_DFL &&
            sigismember(&defaultIgnoredSignals, siginfo.si_signo))) {
        // Discard the ignored signal.
        return;
    }

    if (!pendingSignals || isMoreImportantSignalThan(siginfo.si_signo,
            pendingSignals->siginfo.si_signo)) {
        if (unlikely(pendingSignals &&
                pendingSignals->siginfo.si_signo == siginfo.si_signo)) {
            // Discard the already pending signal.
            return;
        }

        PendingSignal* pending = new PendingSignal;
        pending->siginfo = siginfo;
        pending->next = pendingSignals;
        pendingSignals = pending;
    } else {
        PendingSignal* current = pendingSignals;

        while (current->next && isMoreImportantSignalThan(
                current->next->siginfo.si_signo, siginfo.si_signo)) {
            current = current->next;
        }

        if (unlikely(current->siginfo.si_signo == siginfo.si_signo)) {
            // Discard the already pending signal.
            return;
        }

        PendingSignal* pending = new PendingSignal;
        pending->siginfo = siginfo;
        pending->next = current->next;
        current->next = pending;
    }

    updatePendingSignals();
}

void Thread::updatePendingSignals() {
    PendingSignal* pending = pendingSignals;
    while (pending) {
        if (!sigismember(&signalMask, pending->siginfo.si_signo)) {
            signalPending = true;
            return;
        }
        pending = pending->next;
    }

    signalPending = false;
}

InterruptContext* Signal::sigreturn(InterruptContext* context) {
#ifdef __i386__
    SignalStackFrame* frame = (SignalStackFrame*) context->esp;
    mcontext_t* mcontext = &frame->ucontext.uc_mcontext;

    context->eax = mcontext->__eax;
    context->ebx = mcontext->__ebx;
    context->ecx = mcontext->__ecx;
    context->edx = mcontext->__edx;
    context->esi = mcontext->__esi;
    context->edi = mcontext->__edi;
    context->ebp = mcontext->__ebp;
    context->eip = mcontext->__eip;
    context->eflags = (mcontext->__eflags & 0xCD5) | 0x200;
    context->esp = mcontext->__esp;

#else
#  error "Signal::sigreturn is unimplemented for this architecture."
#endif

    Thread::current()->signalMask = frame->ucontext.uc_sigmask;

    return context;
}

int Syscall::kill(pid_t pid, int signal) {
    if (signal < 0 || signal >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    Process* process;
    if (pid == Process::current()->pid) {
        process = Process::current();
    } else {
        // TODO: Allow sending signals to other processes.
        errno = EPERM;
        return -1;
    }

    if (signal == 0) return 0;

    siginfo_t siginfo = {};
    siginfo.si_signo = signal;
    siginfo.si_code = SI_USER;
    siginfo.si_pid = Process::current()->pid;
    process->raiseSignal(siginfo);

    return 0;
}

int Syscall::sigaction(int signal, const struct sigaction* restrict action,
        struct sigaction* restrict old) {
    if (signal <= 0 || signal >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    if ((signal == SIGKILL || signal == SIGSTOP) &&
            action && action->sa_handler != SIG_DFL) {
        errno = EINVAL;
        return -1;
    }

    if (old) {
        *old = Process::current()->sigactions[signal];
    }

    if (action) {
        Process::current()->sigactions[signal] = *action;
    }

    return 0;
}
