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

/* kernel/src/log.h
 * Defines functions to print to the screen.
 */

#include <stdarg.h>
#include <stdio.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/terminal.h>

static size_t callback(void*, const char* s, size_t nBytes);

void Log::printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vcbprintf(nullptr, callback, format, ap);
    va_end(ap);
}

static size_t callback(void*, const char* s, size_t nBytes) {
    return (size_t) terminal.write(s, nBytes);
}
