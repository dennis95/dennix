/* Copyright (c) 2017, 2019, 2020, 2022, 2023 Dennis Wölfing
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

/* kernel/src/kthread.cpp
 * Kernel threading functions.
 */

#include <errno.h>
#include <sched.h>
#include <dennix/kernel/kthread.h>
#include <dennix/kernel/signal.h>

int kthread_cond_broadcast(kthread_cond_t* cond) {
    kthread_mutex_lock(&cond->mutex);
    while (!cond->waiters.empty()) {
        kthread_cond_waiter& waiter = cond->waiters.front();
        cond->waiters.remove(waiter);
        __atomic_store_n(&waiter.blocked, false, __ATOMIC_RELEASE);
    }
    kthread_mutex_unlock(&cond->mutex);
    return 0;
}

int kthread_cond_sigclockwait(kthread_cond_t* cond, kthread_mutex_t* mutex,
        clockid_t clock, const struct timespec* endTime) {
    kthread_mutex_lock(&cond->mutex);
    kthread_mutex_unlock(mutex);

    kthread_cond_waiter waiter;
    waiter.blocked = true;
    cond->waiters.addBack(waiter);
    kthread_mutex_unlock(&cond->mutex);

    int result = 0;

    while (__atomic_load_n(&waiter.blocked, __ATOMIC_ACQUIRE)) {
        if (endTime) {
            struct timespec now;
            Clock::get(clock)->getTime(&now);
            if (!timespecLess(now, *endTime)) {
                result = ETIMEDOUT;
                break;
            }
        }

        if (Signal::isPending()) {
            result = EINTR;
            break;
        }
        sched_yield();
    }

    if (result) {
        kthread_mutex_lock(&cond->mutex);

        // Only remove the waiter from the list if we were not unblocked
        // concurrently. In that case the waiter was already removed.
        if (__atomic_load_n(&waiter.blocked, __ATOMIC_RELAXED)) {
            cond->waiters.remove(waiter);
        }
        kthread_mutex_unlock(&cond->mutex);
    }

    kthread_mutex_lock(mutex);
    return result;
}

int kthread_cond_signal(kthread_cond_t* cond) {
    kthread_mutex_lock(&cond->mutex);
    if (!cond->waiters.empty()) {
        kthread_cond_waiter& waiter = cond->waiters.front();
        cond->waiters.remove(waiter);
        __atomic_store_n(&waiter.blocked, false, __ATOMIC_RELEASE);
    }
    kthread_mutex_unlock(&cond->mutex);
    return 0;
}

int kthread_cond_sigwait(kthread_cond_t* cond, kthread_mutex_t* mutex) {
    return kthread_cond_sigclockwait(cond, mutex, CLOCK_MONOTONIC, nullptr);
}

int kthread_mutex_lock(kthread_mutex_t* mutex) {
    while (__atomic_test_and_set(mutex, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }
    return 0;
}

int kthread_mutex_trylock(kthread_mutex_t* mutex) {
    if (__atomic_test_and_set(mutex, __ATOMIC_ACQUIRE)) {
        return EBUSY;
    }
    return 0;
}

int kthread_mutex_unlock(kthread_mutex_t* mutex) {
    __atomic_clear(mutex, __ATOMIC_RELEASE);
    return 0;
}
