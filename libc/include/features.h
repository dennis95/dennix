/* Copyright (c) 2016, 2020, 2024 Dennis Wölfing
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

/* libc/include/features.h
 * Feature test macros.
 */

#ifndef _FEATURES_H
#define _FEATURES_H

#define __dennix_libc__ 1

#if !defined(__STRICT_ANSI__) && \
    !defined(_ANSI_SOURCE) && \
    !defined(_ISOC99_SOURCE) && \
    !defined(_ISOC11_SOURCE) && \
    !defined(_POSIX_SOURCE) && \
    !defined(_POSIX_C_SOURCE) && \
    !defined(_DENNIX_SOURCE) && \
    !defined(_ALL_SOURCE) && \
    !defined(_DEFAULT_SOURCE)
#  define _DEFAULT_SOURCE 1
#endif

#if defined(_ALL_SOURCE)
#  if !defined(_ISOC11_SOURCE)
#    define _ISOC11_SOURCE 1
#  endif
#  if !defined(_POSIX_C_SOURCE)
#    define _POSIX_C_SOURCE 202405L
#  endif
#  if !defined(_DENNIX_SOURCE)
#    define _DENNIX_SOURCE 1
#  endif
#endif

#if defined(_DEFAULT_SOURCE) && !defined(_DENNIX_SOURCE)
#  define _DENNIX_SOURCE 1
#endif

#if defined(_ISOC11_SOURCE)
#  define __USE_C 2011
#elif defined(_ISOC99_SOURCE)
#  define __USE_C 1999
#elif defined(_ANSI_SOURCE)
#  define __USE_C 1989
#elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || \
    (defined(__cplusplus) && __cplusplus >= 201703L)
#  define __USE_C 2011
#elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || \
    (defined(__cplusplus) && __cplusplus >= 201103L)
#  define __USE_C 1999
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199409L
#  define __USE_C 1995
#else
#  define __USE_C 1989
#endif

#if defined(_POSIX_C_SOURCE)
#  if _POSIX_C_SOURCE >= 202405L
#    define __USE_POSIX 202405L
#  elif _POSIX_C_SOURCE >= 200809L
#    define __USE_POSIX 200809L
#  elif _POSIX_C_SOURCE >= 200112L
#    define __USE_POSIX 200112L
#  elif _POSIX_C_SOURCE >= 199506L
#    define __USE_POSIX 199506L
#  elif _POSIX_C_SOURCE >= 199309L
#    define __USE_POSIX 199309L
#  elif _POSIX_C_SOURCE >= 2
#    define __USE_POSIX 199209L
#  else
#    define __USE_POSIX 199009L
#  endif
#elif defined(_POSIX_SOURCE)
#  define __USE_POSIX 198808L
#else
#  define __USE_POSIX 0
#endif

#if __USE_POSIX >= 202405L && __USE_C < 2011
#  undef __USE_C
#  define __USE_C 2011
#elif __USE_POSIX >= 200112L && __USE_C < 1999
#  undef __USE_C
#  define __USE_C 1999
#endif

#if defined(_DENNIX_SOURCE)
#  define __USE_DENNIX 1
#else
#  define __USE_DENNIX 0
#endif

#endif
