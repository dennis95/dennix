/* Copyright (c) 2019, 2020 Dennis Wölfing
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

#include <dennix/kernel/log.h>
#include <dennix/kernel/registers.h>

void Registers::dumpInterruptContext(const InterruptContext* context) {
    Log::printf("RAX: 0x%.16lX, RBX: 0x%.16lX, RCX: 0x%.16lX\n", context->rax,
            context->rbx, context->rcx);
    Log::printf("RDX: 0x%.16lX, RSI: 0x%.16lX, RDI: 0x%.16lX\n", context->rdx,
            context->rsi, context->rdi);
    Log::printf("RBP: 0x%.16lX, R8:  0x%.16lX, R9:  0x%.16lX\n", context->rbp,
            context->r8, context->r9);
    Log::printf("R10: 0x%.16lX, R11: 0x%.16lX, R12: 0x%.16lX\n", context->r10,
            context->r11, context->r12);
    Log::printf("R13: 0x%.16lX, R14: 0x%.16lX, R15: 0x%.16lX\n", context->r13,
            context->r14, context->r15);
    Log::printf("ERROR: 0x%.4lX, RFLAGS: 0x%.16lX\n", context->error,
            context->rflags);
    Log::printf("CS:  0x%.4lX, RIP: 0x%.16lX\n", context->cs, context->rip);
    Log::printf("SS:  0x%.4lX, RSP: 0x%.16lX\n", context->ss, context->rsp);
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
    context->rflags = (registers->__rflags & 0xCD5) | 0x200;
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
