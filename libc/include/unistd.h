/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2024 Dennis Wölfing
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
#if __USE_DENNIX || __USE_POSIX >= 202405L
#  include <dennix/oflags.h>
#endif
#if __USE_DENNIX
#  include <dennix/fork.h>
#  include <dennix/meminfo.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Define POSIX version macros. Note that at this point we do not yet provide
   everything required by the options we define. However when an option is
   defined we intend to eventually provide all the missing functionality. */

#if __USE_POSIX != 200809L
#  define _POSIX_VERSION 202405L
#  define _POSIX2_VERSION 202405L
#else
#  define _POSIX_VERSION 200809L
#  define _POSIX2_VERSION 200809L
#endif

/* #define _POSIX_ADVISORY_INFO */
/* TODO: #define _POSIX_ASYNCHRONOUS_IO */
/* TODO: #define _POSIX_BARRIERS */
/* TODO: #define _POSIX_CHOWN_RESTRICTED */
#define _POSIX_CLOCK_SELECTION _POSIX_VERSION
#define _POSIX_CPUTIME _POSIX_VERSION
#define _POSIX_DEVICE_CONTROL 202405L
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
#define _POSIX_THREADS _POSIX_VERSION
/* TODO: #define _POSIX_TIMEOUTS */
#define _POSIX_TIMERS _POSIX_VERSION
/* #define _POSIX_TYPED_MEMORY_OBJECTS */
#define _POSIX2_C_BIND _POSIX2_VERSION
/* #define _POSIX2_C_DEV */
/* #define _POSIX2_CHAR_TERM */
/* #define _POSIX2_FORT_RUN */
/* #define _POSIX2_LOCALEDEF */
/* #define _POSIX2_SW_DEV */
/* #define _POSIX2_UPE */
/* #define _XOPEN_CRYPT */
/* #define _XOPEN_ENH_I18N */
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

#define __SC_UNLIMITED (-1)
#define _SC_ARG_MAX __SC_UNLIMITED
#define _SC_ATEXIT_MAX 1
#define _SC_CHILD_MAX __SC_UNLIMITED
#define _SC_CLK_TICK 2
#define _SC_EXPR_NEST_MAX __SC_UNLIMITED
#define _SC_HOST_NAME_MAX 3
#define _SC_LINE_MAX __SC_UNLIMITED
#define _SC_LOGIN_NAME_MAX __SC_UNLIMITED
#define _SC_NSIG 4
#define _SC_OPEN_MAX __SC_UNLIMITED
#define _SC_PAGESIZE 5
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_THREAD_THREADS_MAX __SC_UNLIMITED
#define _SC_STREAM_MAX __SC_UNLIMITED
#define _SC_SYMLOOP_MAX 6
#define _SC_TTY_NAME_MAX 7
#define _SC_TZNAME_MAX __SC_UNLIMITED

#define _SC_ADVISORY_INFO 1001
#define _SC_ASYNCHRONOUS_IO 1002
#define _SC_BARRIERS 1003
#define _SC_CLOCK_SELECTION 1004
#define _SC_CPUTIME 1005
#define _SC_FSYNC 1006
#define _SC_IPV6 1007
#define _SC_JOB_CONTROL 1008
#define _SC_MAPPED_FILES 1009
#define _SC_MEMLOCK 1010
#define _SC_MEMLOCK_RANGE 1011
#define _SC_MEMORY_PROTECTION 1012
#define _SC_MESSAGE_PASSING 1013
#define _SC_MONOTONIC_CLOCK 1014
#define _SC_PRIORITIZED_IO 1015
#define _SC_PRIORITY_SCHEDULING 1016
#define _SC_RAW_SOCKETS 1017
#define _SC_READER_WRITER_LOCKS 1018
#define _SC_REALTIME_SIGNALS 1019
#define _SC_REGEXP 1020
#define _SC_SAVED_IDS 1021
#define _SC_SEMAPHORES 1022
#define _SC_SHARED_MEMORY_OBJECTS 1023
#define _SC_SHELL 1024
#define _SC_SPAWN 1025
#define _SC_SPIN_LOCKS 1026
#define _SC_SPORADIC_SERVER 1027
#define _SC_SYNCHRONIZED_IO 1028
#define _SC_THREAD_ATTR_STACKADDR 1029
#define _SC_THREAD_ATTR_STACKSIZE 1030
#define _SC_THREAD_CPUTIME 1031
#define _SC_THREAD_PRIO_INHERIT 1032
#define _SC_THREAD_PRIO_PROTECT 1033
#define _SC_THREAD_PRIORITY_SCHEDULING 1034
#define _SC_THREAD_PROCESS_SHARED 1035
#define _SC_THREAD_ROBUST_PRIO_INHERIT 1036
#define _SC_THREAD_ROBUST_PRIO_PROTECT 1037
#define _SC_THREAD_SAFE_FUNCTIONS 1038
#define _SC_THREAD_SPORADIC_SERVER 1039
#define _SC_THREADS 1040
#define _SC_TIMEOUTS 1041
#define _SC_TIMERS 1042
#define _SC_TYPED_MEMORY_OBJECTS 1043
#define _SC_VERSION 1044
#define _SC_V6_ILP32_OFF32 1045
#define _SC_V6_ILP32_OFFBIG 1046
#define _SC_V6_LP64_OFF64 1047
#define _SC_V6_LPBIG_OFFBIG 1048
#define _SC_V7_ILP32_OFF32 1049
#define _SC_V7_ILP32_OFFBIG 1050
#define _SC_V7_LP64_OFF64 1051
#define _SC_V7_LPBIG_OFFBIG 1052
#define _SC_2_C_BIND 1053
#define _SC_2_C_DEV 1054
#define _SC_2_CHAR_TERM 1055
#define _SC_2_FORT_RUN 1056
#define _SC_2_LOCALEDEF 1057
#define _SC_2_SW_DEV 1058
#define _SC_2_UPE 1059
#define _SC_2_VERSION 1060
#define _SC_XOPEN_CRYPT 1061
#define _SC_XOPEN_ENH_I18N 1062
#define _SC_XOPEN_REALTIME 1063
#define _SC_XOPEN_REALTIME_THREADS 1064
#define _SC_XOPEN_SHM 1065
#define _SC_XOPEN_UNIX 1066
#define _SC_XOPEN_UUCP 1067
#define _SC_XOPEN_VERSION 1068

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
int execlp(const char*, const char*, ...);
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
gid_t getegid(void);
uid_t geteuid(void);
gid_t getgid(void);
int gethostname(char*, size_t);
char* getlogin(void);
int getopt(int, char* const[], const char*);
pid_t getpid(void);
pid_t getppid(void);
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
pid_t setsid(void);
unsigned int sleep(unsigned int);
int symlink(const char*, const char*);
int symlinkat(const char*, int, const char*);
long sysconf(int);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
char* ttyname(int);
int unlink(const char*);
int unlinkat(int, const char*, int);
ssize_t write(int, const void*, size_t);

#if __USE_DENNIX || __USE_POSIX >= 202405L
int dup3(int, int, int);
int getentropy(void*, size_t);
int pipe2(int[2], int);
#endif

#if __USE_DENNIX
typedef unsigned long useconds_t;
int fchdirat(int, const char*);
void meminfo(struct meminfo*);
pid_t rfork(int);
pid_t regfork(int, regfork_t*);
int usleep(useconds_t);
#endif

#ifdef __cplusplus
}
#endif

#endif
