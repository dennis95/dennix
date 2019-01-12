/* Copyright (c) 2017, 2019 Dennis Wölfing
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

/* kernel/src/conf.cpp
 * System configuration.
 */

#include <errno.h>
#include <string.h>
#include <dennix/conf.h>
#include <dennix/kernel/syscall.h>

#define SYSNAME "Dennix"
#ifndef DENNIX_VERSION
#  define DENNIX_VERSION "unknown"
#endif
#ifdef __i386__
#  define MACHINE "i686"
#elif defined(__x86_64__)
#  define MACHINE "x86_64"
#else
#  error "Unknown architecture."
#endif

static const char* getConfstr(int name) {
    switch (name) {
    case _CS_UNAME_SYSNAME: return SYSNAME;
    case _CS_UNAME_RELEASE: return DENNIX_VERSION;
    case _CS_UNAME_VERSION: return __DATE__;
    case _CS_UNAME_MACHINE: return MACHINE;
    default:
        return NULL;
    }
}

size_t Syscall::confstr(int name, char* buffer, size_t size) {
    const char* result = getConfstr(name);
    if (!result) {
        errno = EINVAL;
        return 0;
    }

    return strlcpy(buffer, result, size);
}
