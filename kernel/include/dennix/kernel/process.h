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

/* kernel/include/dennix/kernel/process.h
 * Process class.
 */

#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <sys/types.h>
#include <dennix/fork.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/filedescription.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/kthread.h>

#define OPEN_MAX 20

class Process {
public:
    Process();
    ~Process();
    void exit(int status);
    Process* regfork(int flags, struct regfork* registers);
    int execute(const Reference<Vnode>& vnode, char* const argv[],
            char* const envp[]);
    int registerFileDescriptor(FileDescription* descr);
    Process* waitpid(pid_t pid, int flags);
private:
    InterruptContext* interruptContext;
    Process* prev;
    Process* next;
    void* kernelStack;
    bool contextChanged;
    bool fdInitialized;
    bool terminated;
    Process* parent;
    Process** children;
    size_t numChildren;
    kthread_mutex_t childrenMutex;
public:
    AddressSpace* addressSpace;
    FileDescription* fd[OPEN_MAX];
    FileDescription* rootFd;
    FileDescription* cwdFd;
    pid_t pid;
    int status;
    mode_t umask;
public:
    static void addProcess(Process* process);
    static void initialize(FileDescription* rootFd);
    static InterruptContext* schedule(InterruptContext* context);
    static Process* current;
private:
    static int copyArguments(char* const argv[], char* const envp[],
            char**& newArgv, char**& newEnvp, AddressSpace* newAddressSpace);
    static uintptr_t loadELF(uintptr_t elf, AddressSpace* newAddressSpace);
};

void setKernelStack(uintptr_t stack);

#endif
