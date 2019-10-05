/* Copyright (c) 2019 Dennis Wölfing
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

/* libc/src/stdlib/strtof.c
 * Convert a string to float.
 */

#include <endian.h>

#define STRTOD strtof
#define STRTOD_TYPE float
#define STRTOD_MANT_DIG FLT_MANT_DIG
#define STRTOD_MAX_EXP FLT_MAX_EXP
#define STRTOD_MIN_EXP FLT_MIN_EXP
#if BYTE_ORDER == LITTLE_ENDIAN
#  define STRTOD_MAKE_FLOAT(sign, mantissa, exponent) \
    ((union { float f; \
    struct { uint32_t m:23; uint32_t e:8; uint32_t s:1; } i; }) \
    { .i = { .m = (mantissa) & 0x7FFFFF, .e = (exponent) & 0xFF, \
    .s = (sign) } }.f)
#endif

#include "strtod.c"
