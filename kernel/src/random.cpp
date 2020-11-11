/* Copyright (c) 2020 Dennis Wölfing
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

/* kernel/src/random.cpp
 * Randomness.
 */

#include <assert.h>
#include <string.h>
#include <dennix/kernel/clock.h>
#include <dennix/kernel/kernel.h>

static bool initialized;
static bool rdrandSupported;

static void initialize(void) {
#if defined(__i386__) || defined(__x86_64__)
    uint32_t eax = 1;
    uint32_t ecx = 0;
    asm("cpuid" : "+a"(eax), "+c"(ecx) :: "ebx", "edx");
    rdrandSupported = ecx & (1 << 30);
#endif
    initialized = true;
}

extern "C" int getentropy(void* buffer, size_t size) {
    assert(size <= GETENTROPY_MAX);
    if (!initialized) {
        initialize();
    }

    size_t i = 0;
    while (i < size) {
        const void* entropy;
        struct timespec ts[2];
        size_t amount = size - i;
        bool success = false;

#if defined(__i386__) || defined(__x86_64__)
        long value;
        // Use the rdrand instruction if it is available.
        if (rdrandSupported) {
            for (size_t j = 0; j < 10; j++) {
                asm("rdrand %0" : "=r"(value), "=@ccc"(success));
                if (success) break;
            }
            if (amount > sizeof(long)) amount = sizeof(long);
            entropy = &value;
        }
#endif

        if (!success) {
            // If rdrand is not available fall back to using the current time.
            // This is not very secure though.
            // TODO: Gather our own entropy so we don't need to rely on rdrand.
            Clock::get(CLOCK_REALTIME)->getTime(&ts[0]);
            Clock::get(CLOCK_MONOTONIC)->getTime(&ts[1]);
            if (amount > sizeof(ts)) amount = sizeof(ts);
            entropy = ts;
        }
        memcpy((char*) buffer + i, entropy, amount);
        i += amount;
    }
    return 0;
}
