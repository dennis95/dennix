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

/* kernel/src/arch/x86_64/registers.cpp
 * CPU registers.
 */

#include <inttypes.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/registers.h>

void Registers::dumpInterruptContext(const InterruptContext* context) {
    Log::printf("Exception %lu occurred!\n", context->interrupt);
    Log::printf("rax: 0x%lX, rbx: 0x%lX, rcx: 0x%lX\n", context->rax,
            context->rbx, context->rcx);
    Log::printf("rdx: 0x%lX, rdi: 0x%lX, rsi: 0x%lX\n", context->rdx,
            context->rdi, context->rsi);
    Log::printf("rbp: 0x%lX, r8: 0x%lX, r9: 0x%lX\n", context->rbp,
            context->r8, context->r9);
    Log::printf("r10: 0x%lX, r11: 0x%lX, r12: 0x%lX\n", context->r10,
            context->r11, context->r12);
    Log::printf("r13: 0x%lX, r14: 0x%lX, r15: 0x%lX\n", context->r13,
            context->r14, context->r15);
    Log::printf("rip: 0x%lX, cs: 0x%lX, rflags: 0x%lX\n", context->rip,
            context->cs, context->rflags);
    Log::printf("rsp: 0x%lX, ss: 0x%lX, error: 0x%lX\n", context->rsp,
            context->ss, context->error);
}

void Registers::restore(InterruptContext* context,
        const __registers_t* registers) {
    context->rax = registers->__rax;
    context->rbx = registers->__rbx;
    context->rcx = registers->__rcx;
    context->rdx = registers->__rdx;
    context->rsi = registers->__rsi;
    context->rdi = registers->__rdi;
    context->rbp = registers->__rbp;
    context->r8 = registers->__r8;
    context->r9 = registers->__r9;
    context->r10 = registers->__r10;
    context->r11 = registers->__r11;
    context->r12 = registers->__r12;
    context->r13 = registers->__r13;
    context->r14 = registers->__r14;
    context->r15 = registers->__r15;
    context->rip = registers->__rip;
    context->rflags = registers->__rflags;
    context->rsp = registers->__rsp;
    context->cs = 0x1B;
    context->ss = 0x23;
}

void Registers::restoreFpu(const __fpu_t* fpu) {
    asm("fxrstor (%0)" :: "r"(*fpu));
}

void Registers::save(const InterruptContext* context,
        __registers_t* registers) {
    registers->__rax = context->rax;
    registers->__rbx = context->rbx;
    registers->__rcx = context->rcx;
    registers->__rdx = context->rdx;
    registers->__rsi = context->rsi;
    registers->__rdi = context->rdi;
    registers->__rbp = context->rbp;
    registers->__r8 = context->r8;
    registers->__r9 = context->r9;
    registers->__r10 = context->r10;
    registers->__r11 = context->r11;
    registers->__r12 = context->r12;
    registers->__r13 = context->r13;
    registers->__r14 = context->r14;
    registers->__r15 = context->r15;
    registers->__rip = context->rip;
    registers->__rflags = context->rflags;
    registers->__rsp = context->rsp;
}

void Registers::saveFpu(__fpu_t* fpu) {
    asm("fxsave (%0)" :: "r"(*fpu));
}
