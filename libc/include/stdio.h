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

/* libc/include/stdio.h
 * Standard input/output.
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <sys/cdefs.h>
#define __need___va_list
#include <stdarg.h>
#define __need_FILE
#define __need_fpos_t
#define __need_NULL
#define __need_off_t
#define __need_size_t
#if __USE_DENNIX || __USE_POSIX
#  define __need_ssize_t
#endif
#include <sys/libc-types.h>
#include <dennix/seek.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
#define stdin stdin
#define stdout stdout
#define stderr stderr

void clearerr(FILE*);
int fclose(FILE*);
int feof(FILE*);
int ferror(FILE*);
int fflush(FILE*);
int fgetc(FILE*);
char* fgets(char* __restrict, int, FILE* __restrict);
FILE* fopen(const char* __restrict, const char* __restrict);
int fprintf(FILE* __restrict, const char* __restrict, ...) __printf_like(2, 3);
int fputc(int, FILE*);
int fputs(const char* __restrict, FILE* __restrict);
int fseek(FILE*, long, int);
long ftell(FILE*);
size_t fwrite(const void* __restrict, size_t, size_t, FILE* __restrict);
int getc(FILE*);
int getchar(void);
int printf(const char* __restrict, ...) __printf_like(1, 2);
int putc(int, FILE*);
int putchar(int);
int puts(const char*);
int remove(const char*);
int rename(const char*, const char*);
void rewind(FILE*);
int vfprintf(FILE* __restrict, const char* __restrict, __gnuc_va_list)
    __printf_like(2, 0);

#if __USE_DENNIX || __USE_POSIX
FILE* fdopen(int, const char*);
void flockfile(FILE*);
int fseeko(FILE*, off_t, int);
off_t ftello(FILE*);
void funlockfile(FILE*);
int getc_unlocked(FILE*);
int getchar_unlocked(void);
ssize_t getdelim(char** __restrict, size_t* __restrict, int, FILE* __restrict);
ssize_t getline(char** __restrict, size_t* __restrict, FILE* __restrict);
int putc_unlocked(int, FILE*);
int putchar_unlocked(int);
int renameat(int, const char*, int, const char*);
#endif

#if __USE_DENNIX
void clearerr_unlocked(FILE*);
int feof_unlocked(FILE*);
int ferror_unlocked(FILE*);
int fgetc_unlocked(FILE*);
char* fgets_unlocked(char* __restrict, int, FILE* __restrict);
int fputc_unlocked(int, FILE*);
int fputs_unlocked(const char* __restrict, FILE* __restrict);
int fseeko_unlocked(FILE*, off_t, int);
off_t ftello_unlocked(FILE*);
size_t fwrite_unlocked(const void* __restrict, size_t, size_t,
        FILE* __restrict);
int vcbprintf(void*, size_t (*)(void*, const char*, size_t), const char*,
        __gnuc_va_list) __printf_like(3, 0);
int vfprintf_unlocked(FILE* __restrict, const char* __restrict,
        __gnuc_va_list) __printf_like(2, 0);
#endif

/* These are just declared to make libgcov compile, which is compiled with
   libgcc, and are not implemented. */
size_t fread(void* __restrict, size_t, size_t, FILE* __restrict);
void setbuf(FILE* __restrict, char* __restrict);

#ifdef __cplusplus
}
#endif

#endif
