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

/* libc/src/stdlib/strtod.c
 * Convert a string to double.
 * Hexadecimal numbers are always rounded correctly, decimal numbers would
 * require arbitrary precision arithmetic to round correctly.
 */

#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef STRTOD
#  define STRTOD strtod
#  define STRTOD_TYPE double
#  define STRTOD_MANT_DIG DBL_MANT_DIG
#  define STRTOD_MAX_EXP DBL_MAX_EXP
#  define STRTOD_MIN_EXP DBL_MIN_EXP
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define STRTOD_MAKE_FLOAT(sign, mantissa, exponent) \
    ((union { double f; \
    struct { uint64_t m:52; uint64_t e:11; uint64_t s:1; } i; }) \
    { .i = { .m = (mantissa) & 0xFFFFFFFFFFFFF, .e = (exponent) & 0x7FF, \
    .s = (sign) } }.f)
#  endif
#endif

#if !defined(STRTOD_MAKE_FLOAT) || STRTOD_MANT_DIG > 64
#  error "Unsupported floating point format."
#endif

// 64 bit integers are not precise enough to correctly round numbers, therefore
// we use 96 bit integers.
typedef struct {
    uint64_t i64;
    uint32_t i32;
} uint96;

#define INT96_DIG 28 // floor(log_10(2^96))

static void add(uint96* i96, uint64_t i64) {
    i96->i64 += (i64 >> 32) +
            __builtin_add_overflow(i96->i32, i64 & 0xFFFFFFFF, &i96->i32);
}

static void shl(uint96* i96) {
    i96->i64 = (i96->i64 << 1) | (i96->i32 >> 31);
    i96->i32 <<= 1;
}

static void shl4(uint96* i96) {
    i96->i64 = (i96->i64 << 4) | (i96->i32 >> 28);
    i96->i32 <<= 4;
}

static void shr(uint96* i96) {
    i96->i32 = (i96->i32 >> 1) | ((i96->i64 & 1) << 31);
    i96->i64 >>= 1;
}

static void mul10(uint96* i96) {
    uint32_t i32;
    i96->i64 = ((i96->i64 << 3) | (i96->i32 >> 29)) +
            ((i96->i64 << 1) | (i96->i32 >> 31)) +
            __builtin_add_overflow(i96->i32 << 3, i96->i32 << 1, &i32);
    i96->i32 = i32;
}

static void div10(uint96* i96) {
    uint64_t x = (((i96->i64 % 10) << 32) | i96->i32) / 10;
    i96->i64 /= 10;
    i96->i32 = 0;
    add(i96, x);
}

static unsigned int hexValue(char c);
static STRTOD_TYPE makeFloat(bool minus, uint96 x, int64_t y, bool truncated);
static STRTOD_TYPE parseDecimalFloat(const char* restrict str,
        char** restrict end, bool minus);
static int64_t parseExponent(const char* restrict* str);
static STRTOD_TYPE parseHexFloat(const char* restrict str, char** restrict end,
        bool minus);
static bool roundFloat(bool minus, uint96* x, bool truncated);
static bool roundUp(uint96* x);

STRTOD_TYPE STRTOD(const char* restrict string, char** restrict end) {
    const char* str = string;

    while (isspace(*str)) {
        str++;
    }

    bool minus = false;

    if (*str == '+') {
        str++;
    } else if (*str == '-') {
        minus = true;
        str++;
    }

    if ((str[0] == 'i' || str[0] == 'I') &&
            (str[1] == 'n' || str[1] == 'N') &&
            (str[2] == 'f' || str[2] == 'F')) {
        str += 3;
        if ((str[0] == 'i' || str[0] == 'I') &&
                (str[1] == 'n' || str[1] == 'N') &&
                (str[2] == 'i' || str[2] == 'I') &&
                (str[3] == 't' || str[3] == 'T') &&
                (str[4] == 'y' || str[4] == 'Y')) {
            str += 5;
        }
        if (end) {
            *end = (char*) str;
        }
        return minus ? -INFINITY : INFINITY;
    }

    if ((str[0] == 'n' || str[0] == 'N') &&
            (str[1] == 'a' || str[1] == 'A') &&
            (str[2] == 'n' || str[2] == 'N')) {
        str += 3;

        if (*str == '(') {
            const char* s = str + 1;
            while (isalnum(*s) || *s == '_') {
                s++;
            }
            if (*s == ')') {
                str = s + 1;
            }
        }

        if (end) {
            *end = (char*) str;
        }
        return NAN;
    }

    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') &&
            (isxdigit(str[2]) || (str[2] == '.' && isxdigit(str[3])))) {
        str += 2;
        return parseHexFloat(str, end, minus);
    } else if (isdigit(str[0]) || (str[0] == '.' && isdigit(str[1]))) {
        return parseDecimalFloat(str, end, minus);
    }

    if (end) {
        *end = (char*) string;
    }
    return 0;
}

static unsigned int hexValue(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static STRTOD_TYPE makeFloat(bool minus, uint96 x, int64_t y, bool truncated) {
    while (!(x.i64 & 0x8000000000000000)) {
        shl(&x);
        y--;
    }

    y += 95;

    if (y < STRTOD_MIN_EXP - 1) {
        while (y < STRTOD_MIN_EXP - 1) {
            truncated |= x.i32 & 1;
            shr(&x);
            y++;
        }
        y--;
    }

    y += roundFloat(minus, &x, truncated);

    if (y >= STRTOD_MAX_EXP) {
        errno = ERANGE;
        return INFINITY;
    }

    uint64_t m = x.i64 >> (64 - STRTOD_MANT_DIG);
    if (m == 0) {
        // The original number was not 0, so this must have been an underflow.
        errno = ERANGE;
    }

    return STRTOD_MAKE_FLOAT(minus, m, y - STRTOD_MIN_EXP + 2);
}

static STRTOD_TYPE parseDecimalFloat(const char* restrict str,
        char** restrict end, bool minus) {
    while (*str == '0') {
        str++;
    }

    int64_t implicitExponent = 0;
    bool radix = false;

    if (*str == '.') {
        radix = true;
        str++;
        while (*str == '0') {
            implicitExponent--;
            str++;
        }
    }

    uint96 x = {0};
    unsigned int digits = 0;
    bool roundUpOnNonZero = false;
    while (isdigit(*str) || *str == '.') {
        if (*str == '.') {
            if (radix) break;
            radix = true;
            str++;
            continue;
        }
        unsigned int value = *str++ - '0';

        if (digits < INT96_DIG) {
            mul10(&x);
            add(&x, value);
            digits++;

            if (radix) {
                implicitExponent--;
            }
        } else {
            if (!radix) {
                implicitExponent++;
            }

            if (digits == INT96_DIG) {
                digits++;
                // IEEE 754 wants us to correctly round here when we cannot
                // represent more digits exactly.
                int roundingMode = fegetround();
                if (roundingMode == FE_TONEAREST) {
                    roundUpOnNonZero = value >= 5;
                    continue;
#ifdef FE_TONEARESTFROMZERO
                } else if (roundingMode == FE_TONEARESTFROMZERO) {
                    if (value >= 5) {
                        add(&x, 1);
                    }
#endif
                } else if (false
#ifdef FE_UPWARD
                        || (roundingMode == FE_UPWARD && !minus)
#endif
#ifdef FE_DOWNWARD
                        || (roundingMode == FE_DOWNWARD && minus)
#endif
                ) {
                    roundUpOnNonZero = true;
                }
            }

            if (roundUpOnNonZero && value != 0) {
                add(&x, 1);
                roundUpOnNonZero = false;
            }
        }
    }

    int64_t e = 0;
    if ((str[0] == 'e' || str[0] == 'E') &&
            (isdigit(str[1]) ||
            ((str[1] == '+' || str[1] == '-') && isdigit(str[2])))) {
        str++;
        e = parseExponent(&str);
    }

    if (end) {
        *end = (char*) str;
    }

    e += implicitExponent;

    if (x.i32 == 0 && x.i64 == 0) {
        return minus ? -0.0 : 0.0;
    }

    // The floating point number is now represented as x + 10^e + 2^y.
    // We must reduce e to 0 to get a binary representation.
    int64_t y = 0;
    while (e > 0) {
        while (x.i64 & 0xF000000000000000) {
            // The next multiplication by 10 might overflow.
            shr(&x);
            y++;
        }

        e--;
        mul10(&x);
    }

    while (e < 0) {
        // Make the 96bit x as big as possible to avoid rounding errors.
        while (!(x.i64 & 0x8000000000000000)) {
            shl(&x);
            y--;
        }

        e++;
        div10(&x);
    }

    return makeFloat(minus, x, y, false);
}

static int64_t parseExponent(const char* restrict* str) {
    bool minus = false;

    if (**str == '+') {
        (*str)++;
    } else if (**str == '-') {
        minus = true;
        (*str)++;
    }

    int64_t e = 0;
    while (isdigit(**str)) {
        unsigned int value = *(*str)++ - '0';
        e = e * 10 + value;
    }

    return minus ? -e : e;
}

static STRTOD_TYPE parseHexFloat(const char* restrict str, char** restrict end,
        bool minus) {
    while (*str == '0') {
        str++;
    }

    int64_t implicitExponent = 0;
    bool radix = false;

    if (*str == '.') {
        radix = true;
        str++;
        while (*str == '0') {
            implicitExponent -= 4;
            str++;
        }
    }

    const unsigned int maxHexDigits = 96 / 4;
    unsigned int hexDigits = 0;
    bool truncated = false;
    uint96 x = {0};

    while (isxdigit(*str) || *str == '.') {
        if (*str == '.') {
            if (radix) break;
            radix = true;
            str++;
            continue;
        }
        unsigned int value = hexValue(*str++);

        if (hexDigits < maxHexDigits) {
            shl4(&x);
            add(&x, value);
            hexDigits++;

            if (radix) {
                implicitExponent -= 4;
            }
        } else {
            if (value != 0) {
                truncated = true;
            }
            if (!radix) {
                implicitExponent += 4;
            }
        }
    }

    int64_t y = 0;
    if ((str[0] == 'p' || str[0] == 'P') &&
            (isdigit(str[1]) ||
            ((str[1] == '+' || str[1] == '-') && isdigit(str[2])))) {
        str++;
        y = parseExponent(&str);
    }

    if (end) {
        *end = (char*) str;
    }

    y += implicitExponent;

    if (x.i32 == 0 && x.i64 == 0) {
        return minus ? -0.0 : 0.0;
    }

    return makeFloat(minus, x, y, truncated);
}

// Most significant bit is numbered 0.
#define BIT(i96, bit) ((bit) < 64 ? (i96).i64 >> (63 - (bit)) & 1 : \
        (i96).i32 >> (95 - (bit)) & 1)
// True iff the given bit or any less significant bit is set.
#define LOWBITS(i96, bit) !!((bit) < 64 ? ((i96).i64 << (bit)) || (i96).i32 : \
        (i96).i32 << ((bit) - 64))

static bool roundFloat(bool minus, uint96* x, bool truncated) {
    int roundingMode = fegetround();

    // We only have to check for cases where we round up because rounding down
    // is equivalent to truncation which happens automatically.
    if (roundingMode == FE_TONEAREST) {
        if (BIT(*x, STRTOD_MANT_DIG)) {
            if (truncated || BIT(*x, STRTOD_MANT_DIG - 1) ||
                    LOWBITS(*x, STRTOD_MANT_DIG + 1)) {
                return roundUp(x);
            }
        }
#ifdef FE_TONEARESTFROMZERO
    } else if (roundingMode == FE_TONEARESTFROMZERO) {
        if (BIT(*x, STRTOD_MANT_DIG)) {
            return roundUp(x);
        }
#endif
    } else if (false
#ifdef FE_DOWNWARD
            || (roundingMode == FE_DOWNWARD && minus)
#endif
#ifdef FE_UPWARD
            || (roundingMode == FE_UPWARD && !minus)
#endif
    ) {
        if (truncated || LOWBITS(*x, STRTOD_MANT_DIG)) {
            return roundUp(x);
        }
    }

    return false;
}

static bool roundUp(uint96* x) {
    if (__builtin_add_overflow(x->i64, 1ULL << (63 - (STRTOD_MANT_DIG - 1)),
            &x->i64)) {
        x->i64 = 0x8000000000000000;
        return true;
    }
    return false;
}
