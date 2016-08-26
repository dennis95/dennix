/* Copyright (c) 2016, Dennis Wölfing
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

/* kernel/src/syscall.cpp
 * Syscall implementations.
 */

#include <errno.h>
#include <dennix/fcntl.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/syscall.h>

static const void* syscallList[NUM_SYSCALLS] = {
    /*[SYSCALL_EXIT] =*/ (void*) Syscall::exit,
    /*[SYSCALL_WRITE] =*/ (void*) Syscall::write,
    /*[SYSCALL_READ] =*/ (void*) Syscall::read,
    /*[SYSCALL_MMAP] =*/ (void*) Syscall::mmap,
    /*[SYSCALL_MUNMAP] =*/ (void*) Syscall::munmap,
    /*[SYSCALL_OPENAT] =*/ (void*) Syscall::openat,
    /*[SYSCALL_CLOSE] =*/ (void*) Syscall::close,
    /*[SYSCALL_REGFORK] =*/ (void*) Syscall::regfork,
    /*[SYSCALL_EXECVE] =*/ (void*) Syscall::execve,
};

extern "C" const void* getSyscallHandler(unsigned interruptNumber) {
    if (interruptNumber >= NUM_SYSCALLS) {
        return (void*) Syscall::badSyscall;
    } else {
        return syscallList[interruptNumber];
    }
}

int Syscall::close(int fd) {
    FileDescription* descr = Process::current->fd[fd];

    if (!descr) {
        errno = EBADF;
        return -1;
    }

    delete descr;
    Process::current->fd[fd] = nullptr;
    return 0;
}

int Syscall::execve(const char* path, char* const argv[], char* const envp[]) {
    FileDescription* descr = Process::current->rootFd->openat(path, 0, 0);

    if (!descr || Process::current->execute(descr, argv, envp) == -1) {
        return -1;
    }

    // Schedule
    asm volatile ("int $0x31");
    __builtin_unreachable();
}

NORETURN void Syscall::exit(int status) {
    Process::current->exit(status);
    asm volatile ("int $0x31");
    __builtin_unreachable();
}

int Syscall::openat(int fd, const char* path, int flags, mode_t mode) {
    FileDescription* descr;

    if (path[0] == '/') {
        descr = Process::current->rootFd;
    } else if (fd == AT_FDCWD) {
        descr = Process::current->cwdFd;
    } else {
        descr = Process::current->fd[fd];
    }

    FileDescription* result = descr->openat(path, flags, mode);
    if (!result) {
        return -1;
    }
    return Process::current->registerFileDescriptor(result);
}

ssize_t Syscall::read(int fd, void* buffer, size_t size) {
    FileDescription* descr = Process::current->fd[fd];
    return descr->read(buffer, size);
}

pid_t Syscall::regfork(int flags, struct regfork* registers) {
    if (!((flags & RFPROC) && (flags & RFFDG))) {
        errno = EINVAL;
        return -1;
    }

    Process* newProcess = Process::current->regfork(flags, registers);

    return newProcess->pid;
}

ssize_t Syscall::write(int fd, const void* buffer, size_t size) {
    FileDescription* descr = Process::current->fd[fd];
    return descr->write(buffer, size);
}

void Syscall::badSyscall() {
    Log::printf("Syscall::badSyscall was called!\n");
}
