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

/* libc/include/stdlib.h
 * Standard library definitions.
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <sys/cdefs.h>
#define __need_NULL
#define __need_size_t
#define __need_wchar_t
#include <bits/types.h>
#if __USE_DENNIX
#  include <dennix/oflags.h>
#  include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define MB_CUR_MAX 4
#define RAND_MAX 32767

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

__noreturn void abort(void);
int abs(int);
int atexit(void (*)(void));
double atof(const char*);
int atoi(const char*);
long atol(const char*);
void* bsearch(const void*, const void*, size_t, size_t,
        int (*)(const void*, const void*));
void* calloc(size_t, size_t);
div_t div(int, int);
__noreturn void exit(int);
void free(void*);
char* getenv(const char*);
long labs(long);
ldiv_t ldiv(long, long);
void* malloc(size_t);
int mblen(const char*, size_t);
size_t mbstowcs(wchar_t* __restrict, const char* __restrict, size_t);
int mbtowc(wchar_t* __restrict, const char* __restrict, size_t);
void qsort(void*, size_t, size_t, int (*)(const void*, const void*));
int rand(void);
void* realloc(void*, size_t);
void srand(unsigned int);
double strtod(const char* __restrict, char** __restrict);
long strtol(const char* __restrict, char** __restrict, int);
unsigned long strtoul(const char* __restrict, char** __restrict, int);
int system(const char*);

#if __USE_C >= 1999 || __USE_DENNIX
typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

long long atoll(const char*);
__noreturn void _Exit(int);
long long llabs(long long);
lldiv_t lldiv(long long, long long);
float strtof(const char* __restrict, char** __restrict);
long double strtold(const char* __restrict, char** __restrict);
long long strtoll(const char* __restrict, char** __restrict, int);
unsigned long long strtoull(const char* __restrict, char** __restrict, int);
#endif

#if __USE_DENNIX || __USE_POSIX
char* mkdtemp(char*);
int mkstemp(char*);
int setenv(const char*, const char*, int);
int unsetenv(const char*);
#endif

#if __USE_DENNIX
uint32_t arc4random(void);
void arc4random_buf(void*, size_t);
uint32_t arc4random_uniform(uint32_t);
char* canonicalize_file_name(const char*);
int mkostemp(char*, int);
int mkostemps(char*, int, int);
int mkstemps(char*, int);
void qsort_r(void*, size_t, size_t, int (*)(const void*, const void*, void*),
        void*);
void* reallocarray(void*, size_t, size_t);
#endif

#ifdef __cplusplus
}
#endif

#endif
