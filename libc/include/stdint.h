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

/* libc/include/stdint.h
 * Standard integer types.
 */

#ifndef _STDINT_H
#define _STDINT_H

/* integer types */
typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;

typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTPTR_TYPE__ intptr_t;

typedef __UINTMAX_TYPE__ uintmax_t;
typedef __INTMAX_TYPE__ intmax_t;

#if !defined(__cplusplus) || __cplusplus >= 201103L || \
    defined(__STDC_LIMIT_MACROS)

/* integer limits */
#  define UINT8_MAX __UINT8_MAX__
#  define UINT16_MAX __UINT16_MAX__
#  define UINT32_MAX __UINT32_MAX__
#  define UINT64_MAX __UINT64_MAX__

#  define INT8_MAX __INT8_MAX__
#  define INT16_MAX __INT16_MAX__
#  define INT32_MAX __INT32_MAX__
#  define INT64_MAX __INT64_MAX__

#  define INT8_MIN (-INT8_MAX - 1)
#  define INT16_MIN (-INT16_MAX - 1)
#  define INT32_MIN (-INT32_MAX - 1)
#  define INT64_MIN (-INT64_MAX - 1)

#  define UINT_LEAST8_MAX __UINT_LEAST8_MAX__
#  define UINT_LEAST16_MAX __UINT_LEAST16_MAX__
#  define UINT_LEAST32_MAX __UINT_LEAST32_MAX__
#  define UINT_LEAST64_MAX __UINT_LEAST64_MAX__

#  define INT_LEAST8_MAX __INT_LEAST8_MAX__
#  define INT_LEAST16_MAX __INT_LEAST16_MAX__
#  define INT_LEAST32_MAX __INT_LEAST32_MAX__
#  define INT_LEAST64_MAX __INT_LEAST64_MAX__

#  define INT_LEAST8_MIN (-INT_LEAST8_MAX - 1)
#  define INT_LEAST16_MIN (-INT_LEAST16_MAX - 1)
#  define INT_LEAST32_MIN (-INT_LEAST32_MAX - 1)
#  define INT_LEAST64_MIN (-INT_LEAST64_MAX - 1)

#  define UINT_FAST8_MAX __UINT_FAST8_MAX__
#  define UINT_FAST16_MAX __UINT_FAST16_MAX__
#  define UINT_FAST32_MAX __UINT_FAST32_MAX__
#  define UINT_FAST64_MAX __UINT_FAST64_MAX__

#  define INT_FAST8_MAX __INT_FAST8_MAX__
#  define INT_FAST16_MAX __INT_FAST16_MAX__
#  define INT_FAST32_MAX __INT_FAST32_MAX__
#  define INT_FAST64_MAX __INT_FAST64_MAX__

#  define INT_FAST8_MIN (-INT_FAST8_MAX - 1)
#  define INT_FAST16_MIN (-INT_FAST16_MAX - 1)
#  define INT_FAST32_MIN (-INT_FAST32_MAX - 1)
#  define INT_FAST64_MIN (-INT_FAST64_MAX - 1)

#  define UINTPTR_MAX __UINTPTR_MAX__
#  define INTPTR_MAX __INTPTR_MAX__
#  define INTPTR_MIN (-__INTPTR_MAX__ - 1)

#  define UINTMAX_MAX __UINTMAX_MAX__
#  define INTMAX_MAX __INTMAX_MAX__
#  define INTMAX_MIN (-__INTMAX_MAX__ - 1)

/* other type limits */
#  define PTRDIFF_MAX __PTRDIFF_MAX__
#  define PTRDIFF_MIN (-__PTRDIFF_MAX__ - 1)

#  define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__
#  define SIG_ATOMIC_MIN __SIG_ATOMIC_MIN__

#  define SIZE_MAX __SIZE_MAX__

#  define WCHAR_MAX __WCHAR_MAX__
#  define WCHAR_MIN __WCHAR_MIN__

#  define WINT_MAX __WINT_MAX__
#  define WINT_MIN __WINT_MIN__

#endif

#if !defined(__cplusplus) || __cplusplus >= 201103L || \
    defined(__STDC_CONSTANT_MACROS)

/* integer constant expressions */
#  define UINT8_C(value) __UINT8_C(value)
#  define UINT16_C(value) __UINT16_C(value)
#  define UINT32_C(value) __UINT32_C(value)
#  define UINT64_C(value) __UINT64_C(value)

#  define INT8_C(value) __INT8_C(value)
#  define INT16_C(value) __INT16_C(value)
#  define INT32_C(value) __INT32_C(value)
#  define INT64_C(value) __INT64_C(value)

#  define UINTMAX_C(value) __UINTMAX_C(value)
#  define INTMAX_C(value) __INTMAX_C(value)

#endif

#endif
