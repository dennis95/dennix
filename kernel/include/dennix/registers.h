/* Copyright (c) 2019 Dennis Wölfing
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

/* kernel/include/dennix/registers.h
 * CPU registers.
 */

#ifndef _DENNIX_REGISTERS_H
#define _DENNIX_REGISTERS_H

typedef unsigned long __reg_t;

typedef struct {
#ifdef __i386__
    __reg_t __eax;
    __reg_t __ebx;
    __reg_t __ecx;
    __reg_t __edx;
    __reg_t __esi;
    __reg_t __edi;
    __reg_t __ebp;
    __reg_t __eip;
    __reg_t __eflags;
    __reg_t __esp;
#else
#  error "__registers_t is undefined for this architecture."
#endif
} __registers_t;

#ifdef __i386__
typedef char __fpu_t[108];
#else
#  error "__fpu_t is undefined for this architecture."
#endif

#endif
