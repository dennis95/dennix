/* Copyright (c) 2016, 2017, 2018, 2022 Dennis Wölfing
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

/* libc/include/sys/cdefs.h
 * Internal definitions for the standard library.
 */

#ifndef _SYS_CDEFS_H
#define _SYS_CDEFS_H

#include <features.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define __noreturn _Noreturn
#else
# define __noreturn __attribute__((__noreturn__))
#endif

#define __printf_like(format, firstArg) \
    __attribute__((__format__(__printf__, format, firstArg)))
#define __scanf_like(format, firstArg) \
    __attribute__((__format__(__scanf__, format, firstArg)))

/* We use the GNU inline semantics instead of the ISO C inline semantics for
   inline functions in libc, so that applications can redeclare a function
   without accidentally emitting code for it. */
#define __gnu_inline __inline__ __attribute__((__gnu_inline__))

#define __weak_alias(target, alias) \
    extern __typeof(target) alias __attribute__((__weak__, __alias__(#target)))

#endif
