/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/src/thread/pthread_mutexattr.c
 * Mutex attributes. (POSIX2008)
 */

#include "thread.h"

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    (void) attr;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* restrict attr,
        int* restrict type) {
    *type = *attr;
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    *attr = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (type != _MUTEX_NORMAL && type != _MUTEX_RECURSIVE) {
        return EINVAL;
    }
    *attr = type;
    return 0;
}
