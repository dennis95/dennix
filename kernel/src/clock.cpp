/* Copyright (c) 2018 Dennis Wölfing
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

/* kernel/src/clock.cpp
 * System clocks.
 */

#include <errno.h>
#include <dennix/kernel/clock.h>
#include <dennix/kernel/process.h>

static Clock monotonicClock;
static Clock realtimeClock;

Clock::Clock() {
    value.tv_sec = 0;
    value.tv_nsec = 0;
}

Clock* Clock::get(clockid_t clockid) {
    switch (clockid) {
    case CLOCK_MONOTONIC: return &monotonicClock;
    case CLOCK_REALTIME: return &realtimeClock;
    case CLOCK_PROCESS_CPUTIME_ID: return &Process::current->cpuClock;
    default:
        errno = EINVAL;
        return nullptr;
    }
}

int Clock::getTime(struct timespec* result) {
    *result = value;
    return 0;
}

int Clock::setTime(struct timespec* newValue) {
    value = *newValue;
    return 0;
}

void Clock::tick(unsigned long nanoseconds) {
    value.tv_nsec += nanoseconds;

    while (value.tv_nsec >= 1000000000L) {
        value.tv_sec++;
        value.tv_nsec -= 1000000000L;
    }
}

void Clock::onTick(unsigned long nanoseconds) {
    monotonicClock.tick(nanoseconds);
    realtimeClock.tick(nanoseconds);
    Process::current->cpuClock.tick(nanoseconds);
}
