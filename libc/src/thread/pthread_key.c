/* Copyright (c) 2023 Dennis Wölfing
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

/* libc/src/thread/pthread_key.c
 * Thread-specific data. (POSIX2008, called from C11)
 */

#include "thread.h"
#include <stdbool.h>

static __mutex_t mutex = _MUTEX_INIT(_MUTEX_NORMAL);
static void (*destructors[PTHREAD_KEYS_MAX])(void*);

static void noop(void* value) {
    (void) value;
}

int __key_create(__key_t* key, void (*destructor)(void*)) {
    if (!destructor) {
        destructor = noop;
    }

    __mutex_lock(&mutex);

    for (__key_t i = 0; i < PTHREAD_KEYS_MAX; i++) {
        if (destructors[i] == NULL) {
            destructors[i] = destructor;
            *key = i;
            __mutex_unlock(&mutex);
            return 0;
        }
    }

    __mutex_unlock(&mutex);
    return EAGAIN;
}
__weak_alias(__key_create, pthread_key_create);

int __key_delete(__key_t key) {
    __mutex_lock(&mutex);
    __mutex_lock(&__threadListMutex);

    for (__thread_t thread = __threadList; thread; thread = thread->next) {
        thread->keyValues[key] = NULL;
    }
    __mutex_unlock(&__threadListMutex);

    destructors[key] = NULL;
    __mutex_unlock(&mutex);
    return 0;
}
__weak_alias(__key_delete, pthread_key_delete);

void* __key_getspecific(__key_t key) {
    __thread_t self = __thread_self();
    return self->keyValues[key];
}
__weak_alias(__key_getspecific, pthread_getspecific);
__weak_alias(__key_getspecific, tss_get);

int __key_setspecific(__key_t key, const void* value) {
    __thread_t self = __thread_self();
    self->keyValues[key] = (void*) value;
    return 0;
}
__weak_alias(__key_setspecific, pthread_setspecific);

void __key_run_destructors(void) {
    __thread_t self = __thread_self();

    __mutex_lock(&mutex);

    for (int i = 0; i < PTHREAD_DESTRUCTOR_ITERATIONS; i++) {
        bool destructorsRun = false;
        for (__key_t i = 0; i < PTHREAD_KEYS_MAX; i++) {
            if (self->keyValues[i]) {
                destructorsRun = true;
                void* value = self->keyValues[i];
                self->keyValues[i] = NULL;

                void (*destructor)(void*) = destructors[i];
                __mutex_unlock(&mutex);
                destructor(value);
                __mutex_lock(&mutex);
            }
        }

        if (!destructorsRun) break;
    }

    __mutex_unlock(&mutex);
}
