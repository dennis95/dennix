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

/* kernel/src/syscall.cpp
 * Syscall implementations.
 */

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/fcntl.h>
#include <dennix/wait.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/clock.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/pipe.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/symlink.h>
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
    /*[SYSCALL_WAITPID] =*/ (void*) Syscall::waitpid,
    /*[SYSCALL_FSTATAT] =*/ (void*) Syscall::fstatat,
    /*[SYSCALL_READDIR] =*/ (void*) Syscall::readdir,
    /*[SYSCALL_CLOCK_NANOSLEEP] =*/ (void*) Syscall::clock_nanosleep,
    /*[SYSCALL_TCGETATTR] =*/ (void*) Syscall::tcgetattr,
    /*[SYSCALL_TCSETATTR] =*/ (void*) Syscall::tcsetattr,
    /*[SYSCALL_FCHDIRAT] =*/ (void*) Syscall::fchdirat,
    /*[SYSCALL_CONFSTR] =*/ (void*) Syscall::confstr,
    /*[SYSCALL_FSTAT] =*/ (void*) Syscall::fstat,
    /*[SYSCALL_MKDIRAT] =*/ (void*) Syscall::mkdirat,
    /*[SYSCALL_UNLINKAT] =*/ (void*) Syscall::unlinkat,
    /*[SYSCALL_RENAMEAT] =*/ (void*) Syscall::renameat,
    /*[SYSCALL_LINKAT] =*/ (void*) Syscall::linkat,
    /*[SYSCALL_SYMLINKAT] =*/ (void*) Syscall::symlinkat,
    /*[SYSCALL_GETPID] =*/ (void*) Syscall::getpid,
    /*[SYSCALL_KILL] =*/ (void*) Syscall::kill,
    /*[SYSCALL_SIGACTION] =*/ (void*) Syscall::sigaction,
    /*[SYSCALL_ABORT] =*/ (void*) Syscall::abort,
    /*[SYSCALL_CLOCK_GETTIME] =*/ (void*) Syscall::clock_gettime,
    /*[SYSCALL_DUP3] =*/ (void*) Syscall::dup3,
    /*[SYSCALL_ISATTY] =*/ (void*) Syscall::isatty,
    /*[SYSCALL_PIPE2] =*/ (void*) Syscall::pipe2,
    /*[SYSCALL_LSEEK] =*/ (void*) Syscall::lseek,
    /*[SYSCALL_UMASK] =*/ (void*) Syscall::umask,
    /*[SYSCALL_FCHMODAT] =*/ (void*) Syscall::fchmodat,
    /*[SYSCALL_FCNTL] =*/ (void*) Syscall::fcntl,
    /*[SYSCALL_UTIMENSAT] =*/ (void*) Syscall::utimensat,
    /*[SYSCALL_DEVCTL] =*/ (void*) Syscall::devctl,
};

static Reference<FileDescription> getRootFd(int fd, const char* path) {
    if (path[0] == '/') {
        return Process::current()->rootFd;
    } else if (fd == AT_FDCWD) {
        return Process::current()->cwdFd;
    } else {
        return Process::current()->getFd(fd);
    }
}

static Reference<Vnode> resolvePathExceptLastComponent(int fd, char* path,
        char** lastComponent) {
    Reference<FileDescription> descr = getRootFd(fd, path);
    if (!descr) return nullptr;
    return resolvePathExceptLastComponent(descr->vnode, path, lastComponent);
}

extern "C" const void* getSyscallHandler(unsigned interruptNumber) {
    if (interruptNumber >= NUM_SYSCALLS) {
        return (void*) Syscall::badSyscall;
    } else {
        return syscallList[interruptNumber];
    }
}

NORETURN void Syscall::abort() {
    siginfo_t siginfo = {};
    siginfo.si_signo = SIGABRT;
    Process::current()->terminateBySignal(siginfo);

    sched_yield();
    __builtin_unreachable();
}

int Syscall::clock_gettime(clockid_t clockid, struct timespec* result) {
    Clock* clock = Clock::get(clockid);
    if (!clock) return -1;

    return clock->getTime(result);
}

int Syscall::clock_nanosleep(clockid_t clockid, int flags,
        const struct timespec* requested, struct timespec* remaining) {
    if (clockid == CLOCK_PROCESS_CPUTIME_ID) return errno = EINVAL;

    if (clockid == CLOCK_REALTIME && !(flags & TIMER_ABSTIME)) {
        clockid = CLOCK_MONOTONIC;
    }

    Clock* clock = Clock::get(clockid);
    if (!clock) return errno;

    return clock->nanosleep(flags, requested, remaining);
}

int Syscall::close(int fd) {
    return Process::current()->close(fd);
}

int Syscall::devctl(int fd, int command, void* restrict data, size_t size,
        int* restrict info) {
    int dummy;
    if (!info) {
        // Set info so that drivers can assign info unconditionally.
        info = &dummy;
    }

    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) {
        *info = -1;
        return errno;
    }

    return descr->vnode->devctl(command, data, size, info);
}

int Syscall::dup3(int fd1, int fd2, int flags) {
    return Process::current()->dup3(fd1, fd2, flags);
}

int Syscall::execve(const char* path, char* const argv[], char* const envp[]) {
    Reference<FileDescription> descr = getRootFd(AT_FDCWD, path);
    Reference<Vnode> vnode = resolvePath(descr->vnode, path);

    if (!vnode || Process::current()->execute(vnode, argv, envp) == -1) {
        return -1;
    }

    sched_yield();
    __builtin_unreachable();
}

NORETURN void Syscall::exit(int status) {
    Process::current()->exit(status);
    sched_yield();
    __builtin_unreachable();
}

int Syscall::fchdirat(int fd, const char* path) {
    Reference<FileDescription> descr = getRootFd(fd, path);
    if (!descr) return -1;
    Reference<FileDescription> newCwd = descr->openat(path, 0, 0);
    if (!newCwd) return -1;
    if (!S_ISDIR(newCwd->vnode->stat().st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    Process::current()->cwdFd = newCwd;
    return 0;
}

int Syscall::fchmodat(int fd, const char* path, mode_t mode, int flags) {
    if (mode & ~07777) {
        errno = EINVAL;
        return -1;
    }

    bool followFinalSymlink = !(flags & AT_SYMLINK_NOFOLLOW);
    Reference<FileDescription> descr = getRootFd(fd, path);
    if (!descr) return -1;
    Reference<Vnode> vnode = resolvePath(descr->vnode, path,
            followFinalSymlink);
    if (!vnode) return -1;

    return vnode->chmod(mode);
}

int Syscall::fcntl(int fd, int cmd, int param) {
    return Process::current()->fcntl(fd, cmd, param);
}

int Syscall::fstat(int fd, struct stat* result) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->vnode->stat(result);
}

int Syscall::fstatat(int fd, const char* restrict path,
        struct stat* restrict result, int flags) {
    bool followFinalSymlink = !(flags & AT_SYMLINK_NOFOLLOW);
    Reference<FileDescription> descr = getRootFd(fd, path);
    if (!descr) return -1;
    Reference<Vnode> vnode = resolvePath(descr->vnode, path,
            followFinalSymlink);
    if (!vnode) return -1;

    return vnode->stat(result);
}

pid_t Syscall::getpid() {
    return Process::current()->pid;
}

int Syscall::isatty(int fd) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->vnode->isatty();
}

int Syscall::linkat(int oldFd, const char* oldPath, int newFd,
        const char* newPath, int flags) {
    bool followFinalSymlink = flags & AT_SYMLINK_FOLLOW;
    Reference<FileDescription> descr = getRootFd(oldFd, oldPath);
    if (!descr) return -1;
    Reference<Vnode> vnode = resolvePath(descr->vnode, oldPath,
            followFinalSymlink);
    if (!vnode) return -1;

    if (S_ISDIR(vnode->stat().st_mode)) {
        errno = EPERM;
        return -1;
    }

    char* name;
    char* path2Copy = strdup(newPath);
    if (!path2Copy) return -1;
    Reference<Vnode> directory = resolvePathExceptLastComponent(newFd,
            path2Copy, &name);
    if (!directory) {
        free(path2Copy);
        return -1;
    }
    int result = directory->link(name, vnode);
    free(path2Copy);
    return result;
}

off_t Syscall::lseek(int fd, off_t offset, int whence) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->lseek(offset, whence);
}

static void* mmapImplementation(void* /*addr*/, size_t size,
        int protection, int flags, int /*fd*/, off_t /*offset*/) {
    if (size == 0 || !(flags & MAP_PRIVATE)) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (flags & MAP_ANONYMOUS) {
        AddressSpace* addressSpace = Process::current()->addressSpace;
        return (void*) addressSpace->mapMemory(size, protection);
    }

    //TODO: Implement other flags than MAP_ANONYMOUS
    errno = ENOTSUP;
    return MAP_FAILED;
}

int Syscall::mkdirat(int fd, const char* path, mode_t mode) {
    char* pathCopy = strdup(path);
    if (!pathCopy) return -1;

    char* name;
    Reference<Vnode> vnode = resolvePathExceptLastComponent(fd, pathCopy,
            &name);
    if (!vnode) {
        free(pathCopy);
        return -1;
    }
    if (!*name) {
        free(pathCopy);
        errno = EEXIST;
        return -1;
    }

    int result = vnode->mkdir(name, mode & ~Process::current()->umask);
    free(pathCopy);
    return result;
}

void* Syscall::mmap(__mmapRequest* request) {
    return mmapImplementation(request->_addr, request->_size,
            request->_protection, request->_flags, request->_fd,
            request->_offset);
}

int Syscall::munmap(void* addr, size_t size) {
    if (size == 0 || ((vaddr_t) addr & 0xFFF)) {
        errno = EINVAL;
        return -1;
    }

    AddressSpace* addressSpace = Process::current()->addressSpace;
    //TODO: The userspace process could unmap kernel pages!
    addressSpace->unmapMemory((vaddr_t) addr, size);
    return 0;
}

int Syscall::openat(int fd, const char* path, int flags, mode_t mode) {
    Reference<FileDescription> descr = getRootFd(fd, path);
    if (!descr) return -1;

    Reference<FileDescription> result = descr->openat(path, flags,
            mode & ~Process::current()->umask);
    if (!result) return -1;

    int fdFlags = 0;
    if (flags & O_CLOEXEC) fdFlags |= FD_CLOEXEC;

    return Process::current()->addFileDescriptor(result, fdFlags);
}

int Syscall::pipe2(int fd[2], int flags) {
    Reference<Vnode> readPipe;
    Reference<Vnode> writePipe;
    new PipeVnode(readPipe, writePipe);

    Reference<FileDescription> readDescr = new FileDescription(readPipe,
            O_RDONLY);
    Reference<FileDescription> writeDescr = new FileDescription(writePipe,
            O_WRONLY);

    int fdFlags = 0;
    if (flags & O_CLOEXEC) fdFlags |= FD_CLOEXEC;

    int fd0 = Process::current()->addFileDescriptor(readDescr, fdFlags);
    if (fd0 < 0) return -1;
    int fd1 = Process::current()->addFileDescriptor(writeDescr, fdFlags);
    if (fd1 < 0) {
        int oldErrno = errno;
        Process::current()->close(fd0);
        errno = oldErrno;
        return -1;
    }

    fd[0] = fd0;
    fd[1] = fd1;
    return 0;
}

ssize_t Syscall::read(int fd, void* buffer, size_t size) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->read(buffer, size);
}

ssize_t Syscall::readdir(int fd, unsigned long offset, void* buffer,
        size_t size) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->readdir(offset, buffer, size);
}

pid_t Syscall::regfork(int flags, struct regfork* registers) {
    if (!((flags & RFPROC) && (flags & RFFDG))) {
        errno = EINVAL;
        return -1;
    }

    Process* newProcess = Process::current()->regfork(flags, registers);

    return newProcess->pid;
}

int Syscall::renameat(int oldFd, const char* oldPath, int newFd,
        const char* newPath) {
    char* oldCopy = strdup(oldPath);
    if (!oldCopy) return -1;

    char* oldName;
    Reference<Vnode> oldDirectory = resolvePathExceptLastComponent(oldFd,
            oldCopy, &oldName);
    if (!oldDirectory) {
        free(oldCopy);
        return -1;
    }

    char* newCopy = strdup(newPath);
    if (!newCopy) {
        free(oldCopy);
        return -1;
    }

    char* newName;
    Reference<Vnode> newDirectory = resolvePathExceptLastComponent(newFd,
            newCopy, &newName);
    if (!newDirectory) {
        free(oldCopy);
        free(newCopy);
        return -1;
    }

    if (strcmp(oldName, ".") == 0 || strcmp(oldName, "..") == 0 ||
            strcmp(newName, ".") == 0 || strcmp(newName, "..") == 0) {
        free(oldCopy);
        free(newCopy);
        errno = EINVAL;
        return -1;
    }

    int result = newDirectory->rename(oldDirectory, oldName, newName);
    free(oldCopy);
    free(newCopy);
    return result;
}

int Syscall::symlinkat(const char* targetPath, int fd, const char* linkPath) {
    if (!*targetPath) {
        errno = ENOENT;
        return -1;
    }

    char* pathCopy = strdup(linkPath);
    if (!pathCopy) return -1;

    char* name;
    Reference<Vnode> vnode = resolvePathExceptLastComponent(fd, pathCopy,
            &name);
    if (!vnode) {
        free(pathCopy);
        return -1;
    }

    Reference<Vnode> symlink = new SymlinkVnode(targetPath,
            vnode->stat().st_dev);
    int result = vnode->link(name, symlink);
    free(pathCopy);
    return result;
}

int Syscall::tcgetattr(int fd, struct termios* result) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->tcgetattr(result);
}

int Syscall::tcsetattr(int fd, int flags, const struct termios* termio) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->tcsetattr(flags, termio);
}

mode_t Syscall::umask(mode_t newMask) {
    mode_t oldMask = Process::current()->umask;
    Process::current()->umask = newMask & 0777;
    return oldMask;
}

int Syscall::unlinkat(int fd, const char* path, int flags) {
    if (!(flags & (AT_REMOVEDIR | AT_REMOVEFILE))) {
        flags |= AT_REMOVEFILE;
    }
    if (path[strlen(path) - 1] == '/') {
        flags &= ~AT_REMOVEFILE;
    }

    char* pathCopy = strdup(path);
    if (!pathCopy) return -1;

    char* name;
    Reference<Vnode> vnode = resolvePathExceptLastComponent(fd, pathCopy,
            &name);
    if (!vnode) {
        free(pathCopy);
        return -1;
    }

    if (unlikely(!*name && vnode == Process::current()->rootFd->vnode)) {
        free(pathCopy);
        errno = EBUSY;
        return -1;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        free(pathCopy);
        errno = EINVAL;
        return -1;
    }

    int result = vnode->unlink(name, flags);
    free(pathCopy);
    return result;
}

int Syscall::utimensat(int fd, const char* path, const struct timespec ts[2],
        int flags) {
    static struct timespec nullTs[2] = {{ 0, UTIME_NOW }, { 0, UTIME_NOW }};
    if (!ts) {
        ts = nullTs;
    }

    if (((ts[0].tv_nsec < 0 || ts[0].tv_nsec >= 1000000000) &&
            ts[0].tv_nsec != UTIME_NOW && ts[0].tv_nsec != UTIME_OMIT) ||
            ((ts[1].tv_nsec < 0 || ts[1].tv_nsec >= 1000000000) &&
            ts[1].tv_nsec != UTIME_NOW && ts[1].tv_nsec != UTIME_OMIT)) {
        errno = EINVAL;
        return -1;
    }

    bool followFinalSymlink = !(flags & AT_SYMLINK_NOFOLLOW);
    Reference<FileDescription> descr = getRootFd(fd, path);
    if (!descr) return -1;
    Reference<Vnode> vnode = resolvePath(descr->vnode, path,
            followFinalSymlink);
    if (!vnode) return -1;

    return vnode->utimens(ts[0], ts[1]);
}

pid_t Syscall::waitpid(pid_t pid, int* status, int flags) {
    Process* process = Process::current()->waitpid(pid, flags);

    if (!process) {
        return -1;
    }

    int reason = (process->terminationStatus.si_code == CLD_EXITED)
            ? _WEXITED : _WSIGNALED;
    *status = _WSTATUS(reason, process->terminationStatus.si_status);
    pid_t result = process->pid;
    delete process;
    return result;
}

ssize_t Syscall::write(int fd, const void* buffer, size_t size) {
    Reference<FileDescription> descr = Process::current()->getFd(fd);
    if (!descr) return -1;
    return descr->write(buffer, size);
}

void Syscall::badSyscall() {
    siginfo_t siginfo = {};
    siginfo.si_signo = SIGSYS;
    siginfo.si_code = SI_KERNEL;
    Thread::current()->raiseSignal(siginfo);
}
