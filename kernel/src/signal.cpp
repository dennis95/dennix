/* Copyright (c) 2017, 2018, 2019 Dennis Wölfing
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
#include <dennix/kernel/registers.h>
#include <dennix/kernel/signal.h>
#include <dennix/kernel/syscall.h>

struct SignalStackFrame {
#ifdef __i386__
    int signoParam;
    siginfo_t* infoParam;
    void* contextParam;
#elif defined(__x86_64__)
    // We don't push parameters on the stack on this architecture.
#else
#  warning "SignalStackFrame may not be complete for this architecture."
#endif
    // These must always be saved on the stack.
    siginfo_t siginfo;
    ucontext_t ucontext;
};

static sigset_t defaultIgnoredSignals = _SIGSET(SIGCHLD) | _SIGSET(SIGURG);
static sigset_t uncatchableSignals = _SIGSET(SIGKILL) | _SIGSET(SIGSTOP);

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

    uintptr_t frameAddress = (context->STACK_POINTER - sizeof(SignalStackFrame))
            & ~0xF;
    SignalStackFrame* frame = (SignalStackFrame*) frameAddress;
    frame->siginfo = siginfo;

    frame->ucontext.uc_link = nullptr;
    frame->ucontext.uc_sigmask = signalMask;
    frame->ucontext.uc_stack.ss_sp = nullptr;
    frame->ucontext.uc_stack.ss_size = 0;
    frame->ucontext.uc_stack.ss_flags = SS_DISABLE;

    Registers::save(context, &frame->ucontext.uc_mcontext.__regs);
    Registers::saveFpu(&frame->ucontext.uc_mcontext.__fpuEnv);

#ifdef __i386__
    frame->signoParam = siginfo.si_signo;
    frame->infoParam = &frame->siginfo;
    frame->contextParam = &frame->ucontext;
    context->eflags &= ~0x400; // Direction Flag
#elif defined(__x86_64__)
    context->rdi = siginfo.si_signo;
    context->rsi = (uintptr_t) &frame->siginfo;
    context->rdx = (uintptr_t) &frame->ucontext;
    context->rflags &= ~0x400; // Direction Flag
#else
#  error "Signal handler parameters are unimplemented for this architecture."
#endif

    uintptr_t* sigreturnPointer = (uintptr_t*) frameAddress - 1;
    *sigreturnPointer = process->sigreturn;
    context->INSTRUCTION_POINTER = (uintptr_t) action.sa_sigaction;
    context->STACK_POINTER = (uintptr_t) sigreturnPointer;

    signalMask |= (action.sa_mask & ~uncatchableSignals)
            | _SIGSET(siginfo.si_signo);
    return context;
}

void Process::raiseSignal(siginfo_t siginfo) {
    mainThread.raiseSignal(siginfo);
}

void Process::raiseSignalForGroup(siginfo_t siginfo) {
    AutoLock lock(&groupMutex);
    assert(!prevInGroup);

    Process* process = this;
    while (process) {
        process->raiseSignal(siginfo);
        process = process->nextInGroup;
    }
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

        // TODO: This should be implemented in a way that cannot fail.
        PendingSignal* pending = xnew PendingSignal;
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

        // TODO: This should be implemented in a way that cannot fail.
        PendingSignal* pending = xnew PendingSignal;
        pending->siginfo = siginfo;
        pending->next = current->next;
        current->next = pending;
    }

    if (this == Thread::current()) {
        updatePendingSignals();
    }
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
    SignalStackFrame* frame = (SignalStackFrame*) context->STACK_POINTER;
    mcontext_t* mcontext = &frame->ucontext.uc_mcontext;

    Registers::restore(context, &mcontext->__regs);
    Registers::restoreFpu(&mcontext->__fpuEnv);

    Thread::current()->signalMask = frame->ucontext.uc_sigmask
            & ~uncatchableSignals;

    return context;
}

int Syscall::kill(pid_t pid, int signal) {
    if (signal < 0 || signal >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    siginfo_t siginfo = {};
    siginfo.si_signo = signal;
    siginfo.si_code = SI_USER;
    siginfo.si_pid = Process::current()->pid;

    if (pid > 0) {
        Process* process = Process::get(pid);
        if (!process) return -1;
        if (signal == 0) return 0;
        process->raiseSignal(siginfo);
    } else if (pid == -1) {
        // TODO: Implement sending signals to all processes.
        errno = EPERM;
        return -1;
    } else {
        pid_t pgid = -pid;
        if (pid == 0) {
            pgid = Process::current()->pgid;
        }

        Process* processGroup = Process::getGroup(pgid);
        if (!processGroup) return -1;
        if (signal == 0) return 0;
        processGroup->raiseSignalForGroup(siginfo);
    }

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

int Syscall::sigprocmask(int how, const sigset_t* restrict set,
        sigset_t* restrict old) {
    if (old) {
        *old = Thread::current()->signalMask;
    }

    if (how == SIG_BLOCK) {
        if (set) {
            Thread::current()->signalMask |= *set & ~uncatchableSignals;
        }
    } else if (how == SIG_UNBLOCK) {
        if (set) {
            Thread::current()->signalMask &= ~*set;
        }
    } else if (how == SIG_SETMASK) {
        if (set) {
            Thread::current()->signalMask = *set & ~uncatchableSignals;
        }
    } else {
        errno = EINVAL;
        return -1;
    }

    if (set) {
        Thread::current()->updatePendingSignals();
    }

    return 0;
}
