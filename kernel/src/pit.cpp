/* Copyright (c) 2016, Dennis Wölfing
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

#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/pit.h>
#include <dennix/kernel/portio.h>

#define PIT_FREQUENCY 1193182 // Hz

#define PIT_PORT_CHANNEL0 0x40
#define PIT_PORT_MODE 0x43

#define PIT_MODE_RATE_GENERATOR 0x4
#define PIT_MODE_LOBYTE_HIBYTE 0x30

// This should fire the timer approximately every 10 ms.
static const unsigned int frequency = 100;
static const uint16_t divider = PIT_FREQUENCY / frequency;
static const unsigned long nanoseconds = 1000000000L / PIT_FREQUENCY * divider;

static void irqHandler(int irq);

void Pit::initialize() {
    Interrupts::irqHandlers[0] = irqHandler;

    outb(PIT_PORT_MODE, PIT_MODE_RATE_GENERATOR | PIT_MODE_LOBYTE_HIBYTE);
    outb(PIT_PORT_CHANNEL0, divider & 0xFF);
    outb(PIT_PORT_CHANNEL0, (divider >> 8) & 0xFF);
}

#define NUM_TIMERS 20
static Timer* timers[NUM_TIMERS] = {0};

void Pit::deregisterTimer(size_t index) {
    timers[index] = nullptr;
}

size_t Pit::registerTimer(Timer* timer) {
    for (size_t i = 0; i < NUM_TIMERS; i++) {
        if (!timers[i]) {
            timers[i] = timer;
            return i;
        }
    }

    // TODO: Dynamically allocate enough space.
    Log::printf("Error: Too many timers\n");
    while (true) asm volatile ("cli; hlt");
}

static void irqHandler(int /*irq*/) {
    for (size_t i = 0; i < NUM_TIMERS; i++) {
        if (timers[i]) {
            timers[i]->advance(nanoseconds);
        }
    }
}
