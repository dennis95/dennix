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

#include <dennix/kernel/log.h>
#include <dennix/kernel/registers.h>

void Registers::dumpInterruptContext(const InterruptContext* context) {
    Log::printf("EAX: 0x%.8lX, EBX: 0x%.8lX, ECX: 0x%.8lX, EDX: 0x%.8lX\n",
            context->eax, context->ebx, context->ecx, context->edx);
    Log::printf("ESI: 0x%.8lX, EDI: 0x%.8lX, EBP: 0x%.8lX\n",
            context->esi, context->edi, context->ebp);
    Log::printf("ERROR: 0x%.4lX, EFLAGS: 0x%.8lX\n", context->error,
            context->eflags);
    Log::printf("CS:  0x%.4lX, EIP: 0x%.8lX\n", context->cs, context->eip);
    if (context->cs != 0x8) {
        Log::printf("SS: 0x%.4lX, ESP: 0x%.8lX\n", context->ss, context->esp);
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
    asm("fxrstor (%0)" :: "r"(*fpu));
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
    asm("fxsave (%0)" :: "r"(*fpu));
}
