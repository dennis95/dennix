/* Copyright (c) 2016, 2019 Dennis Wölfing
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

/* kernel/include/dennix/kernel/interrupts.h
 * Interrupt handling.
 */

#ifndef KERNEL_INTERRUPTS_H
#define KERNEL_INTERRUPTS_H

#include <dennix/registers.h>
#include <dennix/kernel/kernel.h>

struct InterruptContext {
#ifdef __i386__
    __reg_t eax;
    __reg_t ebx;
    __reg_t ecx;
    __reg_t edx;
    __reg_t esi;
    __reg_t edi;
    __reg_t ebp;

    __reg_t interrupt;
    __reg_t error;

    // These are pushed by the cpu when an interrupt happens.
    __reg_t eip;
    __reg_t cs;
    __reg_t eflags;

    // These are only valid if the interrupt came from Ring 3
    __reg_t esp;
    __reg_t ss;
#else
#  error "InterruptContext is undefined for this architecture."
#endif
};

namespace Interrupts {
extern void (*irqHandlers[])(int);

void disable();
void enable();
void initPic();
}


#endif
