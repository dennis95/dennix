/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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

#include <assert.h>
#include <signal.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/console.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/registers.h>
#include <dennix/kernel/signal.h>
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

class IoApic {
public:
    IoApic(paddr_t baseAddress, int interruptBase);
    void unmask(int irq);
private:
    volatile uint32_t* indexRegister;
    volatile uint32_t* dataRegister;
public:
    int interruptBase;
    int maxIrq;
    IoApic* next;
};

static vaddr_t apicMapped;
static int freeIrq = 16;
static IoApic* firstIoApic;
static IrqHandler* irqHandlers[220] = {0};
uint8_t Interrupts::apicId;
bool Interrupts::hasApic;
int Interrupts::isaIrq[16] =
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

IoApic::IoApic(paddr_t baseAddress, int interruptBase) :
        interruptBase(interruptBase) {
    vaddr_t mapping;
    size_t mapSize;

    vaddr_t mapped = kernelSpace->mapUnaligned(baseAddress, 0x14,
            PROT_READ | PROT_WRITE, mapping, mapSize);
    if (!mapped) PANIC("Failed to map I/O APIC");
    indexRegister = (volatile uint32_t*) mapped;
    dataRegister = (volatile uint32_t*) (mapped + 0x10);

    *indexRegister = 1;
    size_t numIrqs = ((*dataRegister >> 16) & 0xFF) + 1;
    maxIrq = interruptBase + numIrqs;

    if (maxIrq >= freeIrq) {
        freeIrq = maxIrq + 1;
    }

    next = nullptr;
}

void IoApic::unmask(int irq) {
    *indexRegister = 0x10 + (irq - interruptBase) * 2;
    *dataRegister = irq < 16 ? irq + 32 : irq - 16 + 51;
    *indexRegister = 0x10 + (irq - interruptBase) * 2 + 1;
    *dataRegister = Interrupts::apicId << 24;
}

void Interrupts::addIrqHandler(int irq, IrqHandler* handler) {
    assert(irq < 220);
    if (firstIoApic) {
        IoApic* ioApic = firstIoApic;

        while (ioApic) {
            if (irq >= ioApic->interruptBase && irq < ioApic->maxIrq) {
                ioApic->unmask(irq);
            }
            ioApic = ioApic->next;
        }
    } else if (irq < 16) {
        if (irq < 8) {
            outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << irq));
        } else {
            outb(PIC2_DATA, inb(PIC2_DATA) & ~(1 << (irq - 8)));
        }
    }

    handler->next = irqHandlers[irq];
    irqHandlers[irq] = handler;
}

int Interrupts::allocateIrq() {
    if (!hasApic) return -1;
    if (freeIrq >= 220) return -1;
    return freeIrq++;
}

void Interrupts::initApic() {
    hasApic = true;

    apicMapped = kernelSpace->mapPhysical(0xFEE00000, PAGESIZE,
            PROT_READ | PROT_WRITE);
    if (!apicMapped) PANIC("Failed to map APIC");

    volatile uint32_t* apicIdRegister =
            (volatile uint32_t*) (apicMapped + 0x20);
    apicId = *apicIdRegister >> 24;

    volatile uint32_t* spuriousInterruptVector =
            (volatile uint32_t*) (apicMapped + 0xF0);
    *spuriousInterruptVector = 0x1FF;
}

void Interrupts::initIoApic(paddr_t baseAddress, int interruptBase) {
    IoApic* ioApic = xnew IoApic(baseAddress, interruptBase);
    ioApic->next = firstIoApic;

    if (!firstIoApic) {
        outb(PIC1_DATA, 0xFF);
    }

    firstIoApic = ioApic;
}

void Interrupts::initPic() {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 32);
    outb(PIC2_DATA, 40);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, 0x1);
    outb(PIC2_DATA, 0x1);

    outb(PIC1_DATA, 0xFB);
    outb(PIC2_DATA, 0xFF);
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
    } else if (context->interrupt == 0xFF) {
        // This is a spurious interrupt. There is nothing to do.
    } else if (context->interrupt <= 47 || context->interrupt >= 51) {
        int irq = context->interrupt <= 47 ? context->interrupt - 32 :
                context->interrupt - 51 + 16;
        IrqHandler* handler = irqHandlers[irq];
        while (handler) {
            handler->func(handler->user, context);
            handler = handler->next;
        }

        if (irq == Interrupts::isaIrq[0]) {
            console->display->update();
            newContext = Thread::schedule(context);
        }

        // Send End of Interrupt
        if (Interrupts::hasApic) {
            volatile uint32_t* eoiRegister =
                    (volatile uint32_t*) (apicMapped + 0xB0);
            *eoiRegister = 0;
        } else {
            if (irq >= 8) {
                outb(PIC2_COMMAND, PIC_EOI);
            }
            outb(PIC1_COMMAND, PIC_EOI);
        }
    } else if (context->interrupt == 0x31) {
        newContext = Thread::schedule(context);
    } else if (context->interrupt == 0x32) {
        newContext = Signal::sigreturn(context);
    }
    return newContext;
}
