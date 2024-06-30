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
#include <bits/types.h>
#include <dennix/seek.h>

#if __USE_POSIX || __USE_DENNIX
/* POSIX requires us to define va_list but <stdarg.h> does not allow us to
   request only va_list. */
#  ifndef _VA_LIST_DEFINED
typedef __gnuc_va_list va_list;
#    define _VA_LIST_DEFINED
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BUFSIZ 4096
#define EOF (-1)

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

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
int fgetpos(FILE* __restrict, fpos_t* __restrict);
char* fgets(char* __restrict, int, FILE* __restrict);
FILE* fopen(const char* __restrict, const char* __restrict);
int fprintf(FILE* __restrict, const char* __restrict, ...) __printf_like(2, 3);
int fputc(int, FILE*);
int fputs(const char* __restrict, FILE* __restrict);
size_t fread(void* __restrict, size_t, size_t, FILE* __restrict);
FILE* freopen(const char* __restrict, const char* __restrict, FILE* __restrict);
int fscanf(FILE* __restrict, const char* __restrict, ...) __scanf_like(2, 3);
int fseek(FILE*, long, int);
int fsetpos(FILE* __restrict, const fpos_t* __restrict);
long ftell(FILE*);
size_t fwrite(const void* __restrict, size_t, size_t, FILE* __restrict);
int getc(FILE*);
int getchar(void);
void perror(const char*);
int printf(const char* __restrict, ...) __printf_like(1, 2);
int putc(int, FILE*);
int putchar(int);
int puts(const char*);
int remove(const char*);
int rename(const char*, const char*);
void rewind(FILE*);
int scanf(const char* __restrict, ...) __scanf_like(1, 2);
void setbuf(FILE* __restrict, char* __restrict);
int setvbuf(FILE* __restrict, char* __restrict, int, size_t);
int sprintf(char* __restrict, const char* __restrict, ...) __printf_like(2, 3);
int sscanf(const char* __restrict, const char* __restrict, ...)
        __scanf_like(2, 3);
FILE* tmpfile(void);
int ungetc(int, FILE*);
int vfprintf(FILE* __restrict, const char* __restrict, __gnuc_va_list)
        __printf_like(2, 0);
int vprintf(const char* __restrict, __gnuc_va_list) __printf_like(1, 0);
int vsprintf(char* __restrict, const char* __restrict, __gnuc_va_list)
        __printf_like(2, 0);

#if __USE_C >= 1999 || __USE_DENNIX
int snprintf(char* __restrict, size_t, const char* __restrict, ...)
        __printf_like(3, 4);
int vfscanf(FILE* __restrict, const char* __restrict, __gnuc_va_list)
        __scanf_like(2, 0);
int vscanf(const char* __restrict, __gnuc_va_list) __scanf_like(1, 0);
int vsnprintf(char* __restrict, size_t, const char* __restrict, __gnuc_va_list)
        __printf_like(3, 0);
int vsscanf(const char* __restrict, const char* __restrict, __gnuc_va_list)
        __scanf_like(2, 0);
#endif

#if __USE_DENNIX || __USE_POSIX
int dprintf(int, const char* __restrict, ...) __printf_like(2, 3);
FILE* fdopen(int, const char*);
int fileno(FILE*);
void flockfile(FILE*);
FILE* fmemopen(void* __restrict, size_t, const char* __restrict);
int fseeko(FILE*, off_t, int);
off_t ftello(FILE*);
void funlockfile(FILE*);
int getc_unlocked(FILE*);
int getchar_unlocked(void);
ssize_t getdelim(char** __restrict, size_t* __restrict, int, FILE* __restrict);
ssize_t getline(char** __restrict, size_t* __restrict, FILE* __restrict);
int pclose(FILE*);
FILE* popen(const char*, const char*);
int putc_unlocked(int, FILE*);
int putchar_unlocked(int);
int renameat(int, const char*, int, const char*);
int vdprintf(int, const char* __restrict, __gnuc_va_list) __printf_like(2, 0);
#endif

#if __USE_DENNIX || __USE_POSIX >= 202405L
int asprintf(char** __restrict, const char* __restrict, ...)
        __printf_like(2, 3);
int vasprintf(char** __restrict, const char* __restrict, __gnuc_va_list)
        __printf_like(2, 0);
#endif

#if __USE_DENNIX
void clearerr_unlocked(FILE*);
int feof_unlocked(FILE*);
int ferror_unlocked(FILE*);
int fflush_unlocked(FILE*);
int fgetc_unlocked(FILE*);
char* fgets_unlocked(char* __restrict, int, FILE* __restrict);
int fputc_unlocked(int, FILE*);
int fputs_unlocked(const char* __restrict, FILE* __restrict);
size_t fread_unlocked(void* __restrict, size_t, size_t, FILE* __restrict);
int fseeko_unlocked(FILE*, off_t, int);
off_t ftello_unlocked(FILE*);
size_t fwrite_unlocked(const void* __restrict, size_t, size_t,
        FILE* __restrict);
int ungetc_unlocked(int, FILE*);
int vcbprintf(void*, size_t (*)(void*, const char*, size_t), const char*,
        __gnuc_va_list) __printf_like(3, 0);
int vcbscanf(void*, int (*)(void*), int (*)(int, void*), const char* __restrict,
        __gnuc_va_list) __scanf_like(4, 0);

int __fmodeflags(const char*);
#endif

#ifdef __cplusplus
}
#endif

#endif
