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
#include <sys/libc-types.h>
#include <dennix/conf.h>
#if __USE_DENNIX
#  include <dennix/fork.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: This should be defined in <limits.h> which is currently provided by
   the compiler. */
#define HOST_NAME_MAX 255

#define F_OK 0
#define R_OK (1 << 0)
#define W_OK (1 << 1)
#define X_OK (1 << 2)

extern char* optarg;
extern int opterr;
extern int optind;
extern int optopt;

int access(const char*, int);
int chdir(const char*);
int close(int);
size_t confstr(int, char*, size_t);
int dup2(int, int);
int execl(const char*, const char*, ...);
int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
__noreturn void _exit(int);
pid_t fork(void);
char* getcwd(char*, size_t);
int gethostname(char*, size_t);
char* getlogin(void);
int getopt(int, char* const[], const char*);
pid_t getpid(void);
int isatty(int);
int link(const char*, const char*);
int linkat(int, const char*, int, const char*, int);
ssize_t read(int, void*, size_t);
int rmdir(const char*);
unsigned int sleep(unsigned int);
int symlink(const char*, const char*);
int symlinkat(const char*, int, const char*);
int unlink(const char*);
int unlinkat(int, const char*, int);
ssize_t write(int, const void*, size_t);

#if __USE_DENNIX
int dup3(int, int, int);
int fchdirat(int, const char*);
pid_t rfork(int);
pid_t regfork(int, struct regfork*);
#endif

/* These are just declared to make libgcov compile, which is compiled with
   libgcc, and are not implemented. */
int execvp(const char*, char* const[]);

#ifdef __cplusplus
}
#endif

#endif
