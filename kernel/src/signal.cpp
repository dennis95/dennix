/* Copyright (c) 2017 Dennis Wölfing
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
    return Process::current->handleSignal(context);
}

InterruptContext* Process::handleSignal(InterruptContext* /*context*/) {
    kthread_mutex_lock(&signalMutex);
    assert(pendingSignals);

    // Choose the next signal.
    // TODO: Implement signal blocking.
    PendingSignal* pending = pendingSignals;
    pendingSignals = pending->next;
    siginfo_t siginfo = pending->siginfo;
    delete pending;

    updatePendingSignals();
    kthread_mutex_unlock(&signalMutex);

    assert(!(true /* is default action */ &&
            sigismember(&defaultIgnoredSignals, siginfo.si_signo)));

    // TODO: Invoke signal handlers.

    terminateBySignal(siginfo);
    sched_yield();
    __builtin_unreachable();
}

void Process::raiseSignal(siginfo_t siginfo) {
    AutoLock lock(&signalMutex);

    if (true /* is default action */ &&
            sigismember(&defaultIgnoredSignals, siginfo.si_signo)) {
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

void Process::updatePendingSignals() {
    // TODO: Implement signal blocking.
    signalPending = (pendingSignals != nullptr);
}

int Syscall::kill(pid_t pid, int signal) {
    if (signal < 0 || signal >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    Process* process;
    if (pid == Process::current->pid) {
        process = Process::current;
    } else {
        // TODO: Allow sending signals to other processes.
        errno = EPERM;
        return -1;
    }

    if (signal == 0) return 0;

    siginfo_t siginfo = {};
    siginfo.si_signo = signal;
    siginfo.si_code = SI_USER;
    siginfo.si_pid = Process::current->pid;
    process->raiseSignal(siginfo);

    return 0;
}
