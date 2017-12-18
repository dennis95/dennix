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

/* libc/include/string.h
 * String and memory functions.
 */

#ifndef _STRING_H
#define _STRING_H

#include <sys/cdefs.h>
#define __need_NULL
#define __need_size_t
#include <sys/libc-types.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
char* strchr(const char*, int);
int strcmp(const char*, const char*);
char* strcpy(char* __restrict, const char* __restrict);
size_t strcspn(const char*, const char*);
char* strerror(int);
size_t strlen(const char*);
int strncmp(const char*, const char*, size_t);
char* strrchr(const char*, int);
size_t strspn(const char*, const char*);
char* strtok(char* __restrict, const char* __restrict);

#if __USE_DENNIX || __USE_POSIX
char* stpcpy(char* __restrict, const char* __restrict);
char* strdup(const char*);
char* strndup(const char*, size_t);
size_t strnlen(const char*, size_t);
char* strsignal(int);
#endif

#if __USE_DENNIX
size_t strlcpy(char* __restrict, const char* __restrict, size_t);
#endif

#ifdef __cplusplus
}
#endif

#endif
