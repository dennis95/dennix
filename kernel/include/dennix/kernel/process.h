/* Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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
    int execute(const Reference<Vnode>& vnode, char* const argv[],
            char* const envp[]);
    int fcntl(int fd, int cmd, int param);
    Reference<FileDescription> getFd(int fd);
    bool isParentOf(Process* process);
    void raiseSignal(siginfo_t siginfo);
    void raiseSignalForGroup(siginfo_t siginfo);
    Process* regfork(int flags, regfork_t* registers);
    int setpgid(pid_t pgid);
    void terminateBySignal(siginfo_t siginfo);
    Process* waitpid(pid_t pid, int flags);
private:
    void removeFromGroup();
    void terminate();
public:
    AddressSpace* addressSpace;
    Clock childrenSystemCpuClock;
    Clock childrenUserCpuClock;
    Reference<Terminal> controllingTerminal;
    Clock cpuClock;
    Reference<FileDescription> cwdFd;
    Thread mainThread;
    pid_t pid;
    pid_t pgid;
    Reference<FileDescription> rootFd;
    struct sigaction sigactions[NSIG];
    vaddr_t sigreturn;
    Clock systemCpuClock;
    siginfo_t terminationStatus;
    mode_t umask;
    Clock userCpuClock;
private:
    struct timespec alarmTime;
    kthread_mutex_t childrenMutex;
    kthread_mutex_t groupMutex;
    DynamicArray<FdTableEntry, int> fdTable;
    Process* firstChild;
    Process* nextChild;
    Process* parent;
    Process* prevChild;
    Process* prevInGroup;
    Process* nextInGroup;
    bool terminated;
public:
    static bool addProcess(Process* process);
    static Process* current() { return Thread::current()->process; }
    static Process* get(pid_t pid);
    static Process* getGroup(pid_t pgid);
    static Process* initProcess;
private:
    static int copyArguments(char* const argv[], char* const envp[],
            char**& newArgv, char**& newEnvp, AddressSpace* newAddressSpace);
    static uintptr_t loadELF(uintptr_t elf, AddressSpace* newAddressSpace);
};

#endif
