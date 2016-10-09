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

/* kernel/src/timer.cpp
 * Timers.
 */

#include <dennix/kernel/pit.h>
#include <dennix/kernel/syscall.h>
#include <dennix/kernel/timer.h>

static inline void minus(struct timespec* time, unsigned long nanoseconds) {
    time->tv_nsec -= nanoseconds;
    while (time->tv_nsec < 0) {
        time->tv_sec--;
        time->tv_nsec += 1000000000L;
    }

    if (time->tv_sec < 0) {
        time->tv_sec = 0;
        time->tv_nsec = 0;
    }
}

static inline bool isZero(struct timespec time) {
    return (time.tv_sec == 0 && time.tv_nsec == 0);
}

Timer::Timer(struct timespec time) {
    this->time = time;
    index = 0;
}

void Timer::advance(unsigned long nanoseconds) {
    minus(&time, nanoseconds);
}

void Timer::start() {
    index = Pit::registerTimer(this);
}

void Timer::wait() {
    while (!isZero(time)) {
        asm volatile ("int $0x31");
        __sync_synchronize();
    }

    Pit::deregisterTimer(index);
}

int Syscall::nanosleep(const timespec* requested, timespec* remaining) {
    Timer timer(*requested);
    timer.start();
    timer.wait();

    if (remaining) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }

    return 0;
}
