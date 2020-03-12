/* Copyright (c) 2016, 2018, 2020 Dennis Wölfing
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

/* libc/src/stdlib/strtol.c
 * Converts a string into a number.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef STRTOL_NAME
#  define STRTOL_NAME strtol
#  define STRTOL_RESULT long
#  define STRTOL_RESULT_MAX LONG_MAX
#  define STRTOL_RESULT_MIN LONG_MIN
#endif

#define STRTOL_UNSIGNED (STRTOL_RESULT_MIN == 0)

static STRTOL_RESULT getDigitValue(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'z') {
        return 10 + c - 'a';
    } else if (c >= 'A' && c <= 'Z') {
        return 10 + c - 'A';
    } else {
        return STRTOL_RESULT_MAX;
    }
}

STRTOL_RESULT STRTOL_NAME(const char* restrict string, char** restrict end,
        int base) {
    if (base < 0 || base == 1 || base > 36) {
        errno = EINVAL;
        return 0;
    }

    const unsigned char* str = (const unsigned char*) string;

    // Ignore any leading white space
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

    bool negative = minus && !STRTOL_UNSIGNED;

    // Autodetect the base if necessary
    if (base == 0) {
        if (*str == '0') {
            if ((str[1] == 'x' || str[1] == 'X') && isxdigit(str[2])) {
                base = 16;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }

    if (base == 16 && *str == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    STRTOL_RESULT result = 0;
    bool overflow = false;
    bool numberFound = false;

    while (true) {
        STRTOL_RESULT digit = getDigitValue(*str);

        if (digit >= (STRTOL_RESULT) base) {
            break;
        }

        numberFound = true;

        if (__builtin_mul_overflow(result, base, &result)) {
            overflow = true;
        }

        if (!negative && STRTOL_RESULT_MAX - result >= digit) {
            result += digit;
        } else if (negative && result - STRTOL_RESULT_MIN >= digit) {
            result -= digit;
        } else {
            overflow = true;
        }

        str++;
    }

    if (overflow) {
        if (STRTOL_UNSIGNED || !negative) {
            result = STRTOL_RESULT_MAX;
        } else {
            result = STRTOL_RESULT_MIN;
        }
        errno = ERANGE;

    } else if (minus && STRTOL_UNSIGNED) {
        result = -result;
    }

    if (end) {
        *end = numberFound ? (char*) str : (char*) string;
    }

    return result;
}
