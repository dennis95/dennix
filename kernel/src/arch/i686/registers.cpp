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

/* kernel/src/arch/i686/registers.cpp
 * CPU registers.
 */

#include <inttypes.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/registers.h>

void Registers::dumpInterruptContext(const InterruptContext* context) {
    Log::printf("Exception %lu occurred!\n", context->interrupt);
    Log::printf("eax: 0x%lX, ebx: 0x%lX, ecx: 0x%lX, edx: 0x%lX\n",
            context->eax, context->ebx, context->ecx, context->edx);
    Log::printf("edi: 0x%lX, esi: 0x%lX, ebp: 0x%lX, error: 0x%lX\n",
            context->edi, context->esi, context->ebp, context->error);
    Log::printf("eip: 0x%lX, cs: 0x%lX, eflags: 0x%lX\n",
            context->eip, context->cs, context->eflags);
    if (context->cs != 0x8) {
        Log::printf("ss: 0x%lX, esp: 0x%lX\n", context->ss, context->esp);
    }
}

void Registers::restore(InterruptContext* context,
        const __registers_t* registers) {
    context->eax = registers->__eax;
    context->ebx = registers->__ebx;
    context->ecx = registers->__ecx;
    context->edx = registers->__edx;
    context->esi = registers->__esi;
    context->edi = registers->__edi;
    context->ebp = registers->__ebp;
    context->eip = registers->__eip;
    context->eflags = (registers->__eflags & 0xCD5) | 0x200;
    context->esp = registers->__esp;
    context->cs = 0x1B;
    context->ss = 0x23;
}

void Registers::restoreFpu(const __fpu_t* fpu) {
    asm("frstor (%0)" :: "r"(*fpu));
}

void Registers::save(const InterruptContext* context,
        __registers_t* registers) {
    registers->__eax = context->eax;
    registers->__ebx = context->ebx;
    registers->__ecx = context->ecx;
    registers->__edx = context->edx;
    registers->__esi = context->esi;
    registers->__edi = context->edi;
    registers->__ebp = context->ebp;
    registers->__eip = context->eip;
    registers->__eflags = context->eflags;
    registers->__esp = context->esp;
}

void Registers::saveFpu(__fpu_t* fpu) {
    asm("fnsave (%0)" :: "r"(*fpu));
}
