/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

/* kernel/include/dennix/kernel/process.h
 * Process class.
 */

#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <signal.h>
#include <sys/types.h>
#include <dennix/fork.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/clock.h>
#include <dennix/kernel/dynarray.h>
#include <dennix/kernel/filedescription.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/kthread.h>

struct FdTableEntry {
    Reference<FileDescription> descr;
    int flags;

    operator bool() { return descr; }
};

struct PendingSignal {
    siginfo_t siginfo;
    PendingSignal* next;
};

class Process {
public:
    Process();
    ~Process();
    int addFileDescriptor(const Reference<FileDescription>& descr, int flags);
    int close(int fd);
    void exit(int status);
    int execute(const Reference<Vnode>& vnode, char* const argv[],
            char* const envp[]);
    Reference<FileDescription> getFd(int fd);
    Process* regfork(int flags, struct regfork* registers);
    Process* waitpid(pid_t pid, int flags);

    InterruptContext* handleSignal(InterruptContext* context);
    void raiseSignal(siginfo_t siginfo);
    void terminateBySignal(siginfo_t siginfo);
    void updatePendingSignals();
private:
    void terminate();
private:
    InterruptContext* interruptContext;
    void* kernelStack;
    bool contextChanged;
    bool fdInitialized;
    bool terminated;
    Process* parent;
    Process* firstChild;
    Process* prevChild;
    Process* nextChild;
    kthread_mutex_t childrenMutex;
    PendingSignal* pendingSignals;
    kthread_mutex_t signalMutex;
    vaddr_t sigreturn;
    DynamicArray<FdTableEntry, int> fdTable;
public:
    AddressSpace* addressSpace;
    Reference<FileDescription> rootFd;
    Reference<FileDescription> cwdFd;
    pid_t pid;
    mode_t umask;
    siginfo_t terminationStatus;
    struct sigaction sigactions[NSIG];
    sigset_t signalMask;
    Clock cpuClock;
public:
    static bool addProcess(Process* process);
    static void initialize(const Reference<FileDescription>& rootFd);
    static InterruptContext* schedule(InterruptContext* context);
    static Process* current;
    static Process* initProcess;
private:
    static int copyArguments(char* const argv[], char* const envp[],
            char**& newArgv, char**& newEnvp, AddressSpace* newAddressSpace);
    static uintptr_t loadELF(uintptr_t elf, AddressSpace* newAddressSpace);
};

void setKernelStack(uintptr_t stack);

#endif
