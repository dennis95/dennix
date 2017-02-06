/* Copyright (c) 2016, 2017 Dennis Wölfing
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

/* kernel/src/interrupts.cpp
 * Interrupt handling.
 */

#include <inttypes.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/process.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

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

extern "C" InterruptContext* handleInterrupt(InterruptContext* context) {
    InterruptContext* newContext = context;

    if (context->interrupt <= 31) { // CPU Exception
        Log::printf("Exception %" PRIu32 " occurred!\n", context->interrupt);
        Log::printf("eax: 0x%" PRIX32 ", ebx: 0x%" PRIX32 ", ecx: 0x%" PRIX32
                ", edx: 0x%" PRIX32 "\n",
                context->eax, context->ebx, context->ecx, context->edx);
        Log::printf("edi: 0x%" PRIX32 ", esi: 0x%" PRIX32 ", ebp: 0x%" PRIX32
                ", error: 0x%" PRIX32 "\n",
                context->edi, context->esi, context->ebp, context->error);
        Log::printf("eip: 0x%" PRIX32 ", cs: 0x%" PRIX32 ", eflags: 0x%" PRIX32
                "\n", context->eip, context->cs, context->eflags);
        if (context->cs != 0x8) {
            Log::printf("ss: 0x%" PRIX32 ", esp: 0x%" PRIX32 "\n",
                    context->ss, context->esp);
        }
        // Halt the cpu
        while (true) asm volatile ("cli; hlt");
    } else if (context->interrupt <= 47) { // IRQ
        int irq = context->interrupt - 32;
        if (irq == 0) {
            newContext = Process::schedule(context);
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
        newContext = Process::schedule(context);
    } else {
        Log::printf("Unknown interrupt %u!\n", context->interrupt);
    }
    return newContext;
}
