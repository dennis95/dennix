/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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

/* libc/include/unistd.h
 * POSIX definitions.
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/cdefs.h>
#define __need_gid_t
#define __need_NULL
#define __need_off_t
#define __need_pid_t
#define __need_size_t
#define __need_ssize_t
#define __need_uid_t
#include <bits/types.h>
#include <dennix/conf.h>
#include <dennix/seek.h>
#if __USE_DENNIX
#  include <dennix/fork.h>
#  include <dennix/meminfo.h>
#  include <dennix/oflags.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Define POSIX version macros. Note that at this point we do not yet provide
   everything required by the options we define. However when an option is
   defined we intend to eventually provide all the missing functionality. */

#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION 200809L

/* #define _POSIX_ADVISORY_INFO */
/* TODO: #define _POSIX_ASYNCHRONOUS_IO */
/* TODO: #define _POSIX_BARRIERS */
/* TODO: #define _POSIX_CHOWN_RESTRICTED */
#define _POSIX_CLOCK_SELECTION _POSIX_VERSION
#define _POSIX_CPUTIME _POSIX_VERSION
#define _POSIX_FSYNC _POSIX_VERSION
/* #define _POSIX_IPV6 */
#define _POSIX_JOB_CONTROL _POSIX_VERSION
#define _POSIX_MAPPED_FILES _POSIX_VERSION
/* #define _POSIX_MEMLOCK */
/* #define _POSIX_MEMLOCK_RANGE */
/* TODO: #define _POSIX_MEMORY_PROTECTION */
/* #define _POSIX_MESSAGE_PASSING */
#define _POSIX_MONOTONIC_CLOCK _POSIX_VERSION
#define _POSIX_NO_TRUNC 1
/* #define _POSIX_PRIORITIZED_IO */
/* #define _POSIX_PRIORITY_SCHEDULING */
/* #define _POSIX_RAW_SOCKETS */
/* TODO: #define _POSIX_READER_WRITER_LOCKS */
/* TODO: #define _POSIX_REALTIME_SIGNALS */
/* TODO: #define _POSIX_REGEXP */
/* TODO: #define _POSIX_SAVED_IDS */
/* TODO: #define _POSIX_SEMAPHORES */
/* #define _POSIX_SHARED_MEMORY_OBJECTS */
#define _POSIX_SHELL _POSIX_VERSION
/* #define _POSIX_SPAWN */
/* TODO: #define _POSIX_SPIN_LOCKS */
/* #define _POSIX_SPORADIC_SERVER */
#define _POSIX_SYNCHRONIZED_IO _POSIX_VERSION
/* #define _POSIX_THREAD_ATTR_STACKADDR */
/* #define _POSIX_THREAD_ATTR_STACKSIZE */
#define _POSIX_THREAD_CPUTIME _POSIX_VERSION
/* #define _POSIX_THREAD_PRIO_INHERIT */
/* #define _POSIX_THREAD_PRIO_PROTECT */
/* #define _POSIX_THREAD_PRIORITY_SCHEDULING */
/* #define _POSIX_THREAD_PROCESS_SHARED */
/* #define _POSIX_THREAD_ROBUST_PRIO_INHERIT */
/* #define _POSIX_THREAD_ROBUST_PRIO_PROTECT */
#define _POSIX_THREAD_SAFE_FUNCTIONS _POSIX_VERSION
/* #define _POSIX_THREAD_SPORADIC_SERVER */
/* TODO: #define _POSIX_THREADS */
/* TODO: #define _POSIX_TIMEOUTS */
#define _POSIX_TIMERS _POSIX_VERSION
/* #define _POSIX_TYPED_MEMORY_OBJECTS */
#define _POSIX2_C_BIND _POSIX2_VERSION
/* #define _POSIX2_C_DEV */
/* #define _POSIX2_CHAR_TERM */
/* #define _POSIX2_FORT_DEV */
/* #define _POSIX2_FORT_RUN */
/* #define _POSIX2_LOCALEDEF */
/* #define _POSIX2_SW_DEV */
/* #define _POSIX2_UPE */
/* #define _XOPEN_CRYPT */
/* TODO: #define _XOPEN_ENH_I18N */
/* #define _XOPEN_REALTIME */
/* #define _XOPEN_REALTIME_THREADS */
/* #define _XOPEN_SHM */
/* #define _XOPEN_UNIX */
/* #define _XOPEN_UUCP */

#define _POSIX2_SYMLINKS 1

#define _PC_FILESIZEBITS (-1)
#define _PC_PATH_MAX (-2)
#define _PC_PIPE_BUF (-3)
#define _PC_2_SYMLINKS (-4)
#define _PC_NO_TRUNC (-5)
#define _PC_VDISABLE (-6)

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _POSIX_VDISABLE '\0'

#define F_OK 0
#define R_OK (1 << 0)
#define W_OK (1 << 1)
#define X_OK (1 << 2)

extern char* optarg;
extern int opterr;
extern int optind;
extern int optopt;

int access(const char*, int);
unsigned int alarm(unsigned int);
int chdir(const char*);
int chown(const char*, uid_t, gid_t);
int close(int);
size_t confstr(int, char*, size_t);
int dup(int);
int dup2(int, int);
int execl(const char*, const char*, ...);
int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execvp(const char*, char* const[]);
__noreturn void _exit(int);
int fchdir(int);
int fchown(int, uid_t, gid_t);
int fchownat(int, const char*, uid_t, gid_t, int);
int fdatasync(int);
pid_t fork(void);
long fpathconf(int, int);
int fsync(int);
int ftruncate(int, off_t);
char* getcwd(char*, size_t);
uid_t geteuid(void);
gid_t getgid(void);
int gethostname(char*, size_t);
char* getlogin(void);
int getopt(int, char* const[], const char*);
pid_t getpid(void);
pid_t getpgid(pid_t);
pid_t getpgrp(void);
uid_t getuid(void);
int isatty(int);
int lchown(const char*, uid_t, gid_t);
int link(const char*, const char*);
int linkat(int, const char*, int, const char*, int);
off_t lseek(int, off_t, int);
long pathconf(const char*, int);
int pipe(int[2]);
ssize_t read(int, void*, size_t);
ssize_t readlink(const char* __restrict, char* __restrict, size_t);
ssize_t readlinkat(int, const char* __restrict, char* __restrict, size_t);
int rmdir(const char*);
int setpgid(pid_t, pid_t);
unsigned int sleep(unsigned int);
int symlink(const char*, const char*);
int symlinkat(const char*, int, const char*);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
char* ttyname(int);
int unlink(const char*);
int unlinkat(int, const char*, int);
ssize_t write(int, const void*, size_t);

#if __USE_DENNIX
typedef unsigned long useconds_t;
int dup3(int, int, int);
int fchdirat(int, const char*);
int getentropy(void*, size_t);
void meminfo(struct meminfo*);
int pipe2(int[2], int);
pid_t rfork(int);
pid_t regfork(int, regfork_t*);
int usleep(useconds_t);
#endif

#ifdef __cplusplus
}
#endif

#endif
