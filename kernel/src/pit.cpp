/* Copyright (c) 2016, 2018, 2020, 2021 Dennis Wölfing
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

/* kernel/src/pit.cpp
 * Programmable Interval Timer.
 */

#include <dennix/kernel/clock.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/pit.h>
#include <dennix/kernel/portio.h>

#define PIT_FREQUENCY 1193182 // Hz

#define PIT_PORT_CHANNEL0 0x40
#define PIT_PORT_MODE 0x43

#define PIT_MODE_RATE_GENERATOR 0x4
#define PIT_MODE_LOBYTE_HIBYTE 0x30

// This should fire the timer approximately every millisecond.
static const unsigned int frequency = 1000;
static const uint16_t divider = PIT_FREQUENCY / frequency;
static const unsigned long nanoseconds = 1000000000L / PIT_FREQUENCY * divider;

static void irqHandler(void*, const InterruptContext* context);
static IrqHandler handler;

void Pit::initialize() {
    if (Interrupts::timerIrq != -1) return;

    handler.func = irqHandler;
    Interrupts::addIrqHandler(Interrupts::isaIrq[0], &handler);
    Interrupts::timerIrq = Interrupts::isaIrq[0];

    outb(PIT_PORT_MODE, PIT_MODE_RATE_GENERATOR | PIT_MODE_LOBYTE_HIBYTE);
    outb(PIT_PORT_CHANNEL0, divider & 0xFF);
    outb(PIT_PORT_CHANNEL0, (divider >> 8) & 0xFF);
}

static void irqHandler(void*, const InterruptContext* context) {
    Clock::onTick(context->cs != 0x8, nanoseconds);
}
