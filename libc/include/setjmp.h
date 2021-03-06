/* Copyright (c) 2018, 2019 Dennis Wölfing
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

/* libc/include/setjmp.h
 * Nonlocal jumps.
 */

#ifndef _SETJMP_H
#define _SETJMP_H

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __i386__
typedef unsigned long jmp_buf[6]; /* ebx, esi, edi, ebp, esp, eip */
#elif defined(__x86_64__)
typedef unsigned long jmp_buf[8]; /* rbx, rbp, rsp, r12, r13, r14, r15, rip */
#else
#  error "jmp_buf is undefined for this architecture."
#endif

__noreturn void longjmp(jmp_buf, int);
int setjmp(jmp_buf) __attribute__((__returns_twice__));

#ifdef __cplusplus
}
#endif

#endif
