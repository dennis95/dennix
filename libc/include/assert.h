/* Copyright (c) 2016, 2019 Dennis Wölfing
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

/* libc/include/assert.h
 * Assertions.
 */

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef assert

#ifdef NDEBUG
#  define assert(ignore) ((void) 0)
#else
__noreturn void __assertionFailure(const char*, const char*, unsigned int,
        const char*);
#  define assert(assertion) ((assertion) ? (void) 0 : \
        __assertionFailure(#assertion, __FILE__, __LINE__, __func__))
#endif

#if !defined(__cplusplus) && __STDC_VERSION__ >= 201112L
#  define static_assert _Static_assert
#endif

#ifdef __cplusplus
}
#endif
