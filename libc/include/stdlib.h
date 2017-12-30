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

/* libc/include/stdlib.h
 * Standard library definitions.
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <sys/cdefs.h>
#define __need_NULL
#define __need_size_t
#include <sys/libc-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define RAND_MAX 32767

__noreturn void abort(void);
int atexit(void (*)(void));
void* calloc(size_t, size_t);
__noreturn void _Exit(int);
__noreturn void exit(int);
void free(void*);
char* getenv(const char*);
void* malloc(size_t);
int rand(void);
void* realloc(void*, size_t);
void srand(unsigned int);
long strtol(const char* __restrict, char** __restrict, int);
unsigned long strtoul(const char* __restrict, char** __restrict, int);

#if __USE_DENNIX || __USE_POSIX
int setenv(const char*, const char*, int);
int unsetenv(const char*);
#endif

#if __USE_DENNIX
char* canonicalize_file_name(const char*);
void* reallocarray(void*, size_t, size_t);
#endif

/* These are just declared to make libgcov compile, which is compiled with
   libgcc, and are not implemented. */
int atoi(const char*);

#ifdef __cplusplus
}
#endif

#endif
