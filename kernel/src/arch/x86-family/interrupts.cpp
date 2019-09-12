/* Copyright (c) 2016, 2017, 2018, 2019 Dennis Wölfing
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

/* kernel/src/arch/x86-family/interrupts.cpp
 * Interrupt handling.
 */

#include <signal.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/registers.h>
#include <dennix/kernel/signal.h>
#include <dennix/kernel/terminaldisplay.h>
#include <dennix/kernel/thread.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

#define EX_DIVIDE_BY_ZERO 0
#define EX_DEBUG 1
#define EX_NON_MASKABLE_INTERRUPT 2
#define EX_BREAKPOINT 3
#define EX_OVERFLOW 4
#define EX_BOUND_RANGE_EXCEEDED 5
#define EX_INVALID_OPCODE 6
#define EX_DEVICE_NOT_AVAILABLE 7
#define EX_DOUBLE_FAULT 8
#define EX_COPROCESSOR_SEGMENT_OVERRUN 9
#define EX_INVAILD_TSS 10
#define EX_SEGMENT_NOT_PRESENT 11
#define EX_STACK_SEGMENT_FAULT 12
#define EX_GENERAL_PROTECTION_FAULT 13
#define EX_PAGE_FAULT 14
#define EX_X87_FLOATING_POINT_EXCEPTION 16
#define EX_ALIGNMENT_CHECK 17
#define EX_MACHINE_CHECK 18
#define EX_SIMD_FLOATING_POINT_EXCEPTION 19
#define EX_VIRTUALIZATION_EXCEPTION 20

void (*Interrupts::irqHandlers[16])(int) = {0};

void Interrupts::initPic() {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 32);
    outb(PIC2_DATA, 40);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, 0x1);
    outb(PIC2_DATA, 0x1);
}

void Interrupts::disable() {
    asm volatile ("cli");
}

void Interrupts::enable() {
    asm volatile ("sti");
}

static const char* exceptionName(unsigned int exception) {
    switch (exception) {
    case EX_DIVIDE_BY_ZERO: return "Divide-by-zero Exception";
    case EX_DEBUG: return "Debug Exception";
    case EX_NON_MASKABLE_INTERRUPT: return "Non-maskable Interrupt";
    case EX_BREAKPOINT: return "Breakpoint Exception";
    case EX_OVERFLOW: return "Overflow Exception";
    case EX_BOUND_RANGE_EXCEEDED: return "Bound Range Exceeded Exception";
    case EX_INVALID_OPCODE: return "Invalid Opcode Exception";
    case EX_DEVICE_NOT_AVAILABLE: return "Device Not Available Exception";
    case EX_DOUBLE_FAULT: return "Double Fault";
    case EX_COPROCESSOR_SEGMENT_OVERRUN: return "Coprocessor Segment Overrun";
    case EX_INVAILD_TSS: return "Invalid TSS Exception";
    case EX_SEGMENT_NOT_PRESENT: return "Segment Not Present Exception";
    case EX_STACK_SEGMENT_FAULT: return "Stack Fault";
    case EX_GENERAL_PROTECTION_FAULT: return "General Protection Fault";
    case EX_PAGE_FAULT: return "Page Fault";
    case EX_X87_FLOATING_POINT_EXCEPTION: return "x87 Floating Point Exception";
    case EX_ALIGNMENT_CHECK: return "Alignment Check Exception";
    case EX_MACHINE_CHECK: return "Machine Check Exception";
    case EX_SIMD_FLOATING_POINT_EXCEPTION:
        return "SIMD Floating Point Exception";
    case EX_VIRTUALIZATION_EXCEPTION: return "Virtualization Exception";
    default: return "Exception";
    }
}

static bool handleUserspaceException(const InterruptContext* context) {
    siginfo_t siginfo = {};
    switch (context->interrupt) {
    case EX_DIVIDE_BY_ZERO:
        siginfo.si_signo = SIGFPE;
        siginfo.si_code = FPE_INTDIV;
        siginfo.si_addr = (void*) context->INSTRUCTION_POINTER;
        break;
    case EX_DEBUG:
    case EX_BREAKPOINT:
        siginfo.si_signo = SIGTRAP;
        siginfo.si_code = TRAP_BRKPT;
        siginfo.si_addr = (void*) context->INSTRUCTION_POINTER;
        break;
    case EX_OVERFLOW:
    case EX_BOUND_RANGE_EXCEEDED:
    case EX_STACK_SEGMENT_FAULT:
    case EX_GENERAL_PROTECTION_FAULT:
        siginfo.si_signo = SIGSEGV;
        siginfo.si_code = SI_KERNEL;
        siginfo.si_addr = (void*) context->INSTRUCTION_POINTER;
        break;
    case EX_INVALID_OPCODE:
        siginfo.si_signo = SIGILL;
        siginfo.si_code = ILL_ILLOPC;
        siginfo.si_addr = (void*) context->INSTRUCTION_POINTER;
        break;
    case EX_PAGE_FAULT:
        siginfo.si_signo = SIGSEGV;
        siginfo.si_code = SEGV_MAPERR;
        asm ("mov %%cr2, %0" : "=r"(siginfo.si_addr));
        break;
    case EX_X87_FLOATING_POINT_EXCEPTION:
    case EX_SIMD_FLOATING_POINT_EXCEPTION:
        siginfo.si_signo = SIGFPE;
        siginfo.si_code = FPE_FLTINV;
        siginfo.si_addr = (void*) context->INSTRUCTION_POINTER;
        break;
    default:
        return false;
    }

    Thread::current()->raiseSignal(siginfo);
    return true;
}

extern "C" InterruptContext* handleInterrupt(InterruptContext* context) {
    InterruptContext* newContext = context;
    if (context->interrupt <= 31 && context->cs != 0x8) {
        if (!handleUserspaceException(context)) goto handleKernelException;
    } else if (context->interrupt <= 31) { // CPU Exception
handleKernelException:
        PANIC(context, "Unexpected %s", exceptionName(context->interrupt));
    } else if (context->interrupt <= 47) { // IRQ
        int irq = context->interrupt - 32;
        if (irq == 0) {
            TerminalDisplay::display->update();
            newContext = Thread::schedule(context);
        }

        if (Interrupts::irqHandlers[irq]) {
            Interrupts::irqHandlers[irq](irq);
        }

        // Send End of Interrupt
        if (irq >= 8) {
            outb(PIC2_COMMAND, PIC_EOI);
        }
        outb(PIC1_COMMAND, PIC_EOI);
    } else if (context->interrupt == 0x31) {
        newContext = Thread::schedule(context);
    } else if (context->interrupt == 0x32) {
        newContext = Signal::sigreturn(context);
    } else {
        PANIC(context, "Unexpected interrupt %lu", context->interrupt);
    }
    return newContext;
}
