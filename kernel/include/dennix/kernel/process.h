/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021, 2022 Dennis Wölfing
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
#include <dennix/kernel/dynarray.h>
#include <dennix/kernel/filedescription.h>
#include <dennix/kernel/terminal.h>
#include <dennix/kernel/thread.h>

struct FdTableEntry {
    Reference<FileDescription> descr;
    int flags;

    operator bool() { return descr; }
};

class Process {
    friend Thread;
public:
    Process();
    ~Process();
    int addFileDescriptor(const Reference<FileDescription>& descr, int flags);
    unsigned int alarm(unsigned int seconds);
    int close(int fd);
    int dup3(int fd1, int fd2, int flags);
    void exit(int status);
    int execute(Reference<Vnode>& vnode, char* const argv[],
            char* const envp[]);
    int fcntl(int fd, int cmd, int param);
    Reference<FileDescription> getFd(int fd);
    bool isParentOf(Process* process);
    void raiseSignal(siginfo_t siginfo);
    void raiseSignalForGroup(siginfo_t siginfo);
    Process* regfork(int flags, regfork_t* registers);
    int setpgid(pid_t pgid);
    pid_t setsid();
    void terminateBySignal(siginfo_t siginfo);
    mode_t umask(const mode_t* newMask = nullptr);
    Process* waitpid(pid_t pid, int flags);
private:
    void removeFromGroup();
    void terminate();
public:
    AddressSpace* addressSpace;
    Clock childrenSystemCpuClock;
    Clock childrenUserCpuClock;
    Clock cpuClock;
    Thread mainThread;
    pid_t pid;
    Clock systemCpuClock;
    bool terminated;
    siginfo_t terminationStatus;
    Clock userCpuClock;

    kthread_mutex_t fdMutex;
    Reference<FileDescription> cwdFd;
    Reference<FileDescription> rootFd;

    kthread_mutex_t jobControlMutex;
    Reference<Terminal> controllingTerminal;
    pid_t pgid;
    pid_t sid;

    kthread_mutex_t signalMutex;
    struct sigaction sigactions[NSIG];
private:
    struct timespec alarmTime;
    DynamicArray<FdTableEntry, int> fdTable;
    Process* parent;
    vaddr_t sigreturn;

    kthread_mutex_t childrenMutex;
    Process* firstChild;
    Process* prevChild;
    Process* nextChild;

    kthread_mutex_t fileMaskMutex;
    mode_t fileMask;

    kthread_mutex_t groupMutex;
    Process* prevInGroup;
    Process* nextInGroup;
public:
    static bool addProcess(Process* process);
    static Process* current() { return Thread::current()->process; }
    static Process* get(pid_t pid);
    static Process* getGroup(pid_t pgid);
    static Process* initProcess;
private:
    static int copyArguments(char* const argv[], char* const envp[],
            char**& newArgv, char**& newEnvp, AddressSpace* newAddressSpace);
    static uintptr_t loadELF(const Reference<Vnode>& vnode,
            AddressSpace* newAddressSpace);
};

#endif
