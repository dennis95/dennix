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

/* kernel/include/dennix/kernel/syscall.h
 * Syscall function declarations.
 */

#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <sys/types.h>
#include <dennix/fork.h>
#include <dennix/syscall.h>
#include <dennix/kernel/kernel.h>

struct __mmapRequest;
struct stat;

namespace Syscall {

int close(int fd);
int execve(const char* path, char* const argv[], char* const envp[]);
NORETURN void exit(int status);
int fstatat(int fd, const char* restrict path, struct stat* restrict result,
        int flags);
void* mmap(__mmapRequest* request);
int munmap(void* addr, size_t size);
int openat(int fd, const char* path, int flags, mode_t mode);
ssize_t read(int fd, void* buffer, size_t size);
ssize_t readdir(int fd, unsigned long offset, void* buffer, size_t size);
pid_t regfork(int flags, struct regfork* registers);
pid_t waitpid(pid_t pid, int* status, int flags);
ssize_t write(int fd, const void* buffer, size_t size);

void badSyscall();

}

#endif
