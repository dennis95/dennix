/* Copyright (c) 2017, 2019, 2020, 2021 Dennis Wölfing
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

/* kernel/include/dennix/kernel/kthread.h
 * Kernel threading functions.
 */

#ifndef KERNEL_KTHREAD_H
#define KERNEL_KTHREAD_H

#include <dennix/kernel/clock.h>

typedef bool kthread_mutex_t;
#define KTHREAD_MUTEX_INITIALIZER false

struct kthread_cond_waiter {
    kthread_cond_waiter* prev;
    kthread_cond_waiter* next;
    bool blocked;
};

typedef struct {
    kthread_mutex_t mutex;
    kthread_cond_waiter* first;
    kthread_cond_waiter* last;
} kthread_cond_t;
#define KTHREAD_COND_INITIALIZER { KTHREAD_MUTEX_INITIALIZER, nullptr, nullptr }

int kthread_cond_broadcast(kthread_cond_t* cond);
int kthread_cond_sigclockwait(kthread_cond_t* cond, kthread_mutex_t* mutex,
        clockid_t clock, const struct timespec* endTime);
int kthread_cond_signal(kthread_cond_t* cond);
int kthread_cond_sigwait(kthread_cond_t* cond, kthread_mutex_t* mutex);
int kthread_mutex_lock(kthread_mutex_t* mutex);
int kthread_mutex_trylock(kthread_mutex_t* mutex);
int kthread_mutex_unlock(kthread_mutex_t* mutex);

// A useful class that automatically unlocks a mutex when it goes out of scope.
class AutoLock {
public:
    AutoLock(kthread_mutex_t* mutex) {
        this->mutex = mutex;
        if (mutex) {
            kthread_mutex_lock(mutex);
        }
    }

    ~AutoLock() {
        if (mutex) {
            kthread_mutex_unlock(mutex);
        }
    }
private:
    kthread_mutex_t* mutex;
};

#endif
