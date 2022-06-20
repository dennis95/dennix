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

/* libc/src/stdio/vcbprintf.c
 * Print formatted output. (called from C89)
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// TODO: Support numbered arguments

static const char* lowerDigits = "0123456789abcdef";
static const char* upperDigits = "0123456789ABCDEF";

static int integerToString(char* output, uintmax_t value, unsigned int base,
        const char* digits);
static size_t noop(void*, const char*, size_t);
static int printInteger(void* param,
        size_t (*callback)(void*, const char*, size_t), char specifier,
        uintmax_t value, int fieldWidth, int precision, int flags);
static int printString(void* param,
        size_t (*callback)(void*, const char*, size_t), const char* s,
        int length, int fieldWidth, int flags);

enum {
    // Powers of two N >= 8 represent length modifier wN.
    // N+1 represents length modifier wfN.
    LENGTH_CHAR,
    LENGTH_SHORT,
    LENGTH_INT,
    LENGTH_LONG,
    LENGTH_LONG_LONG,
    LENGTH_INTMAX,
    LENGTH_SIZE,
    LENGTH_PTRDIFF,
    LENGTH_LONG_DOUBLE = 10
};

enum {
    // Thousands grouping is currently not implemented because it has no effect
    // in POSIX locale.
    FLAG_THOUSANDS_GROUPING = 1 << 0,
    FLAG_LEFT_JUSTIFIED = 1 << 1,
    FLAG_PLUS = 1 << 2,
    FLAG_SPACE = 1 << 3,
    FLAG_ALTERNATIVE = 1 << 4,
    FLAG_LEADING_ZEROS = 1 << 5
};

int __vcbprintf(void* param, size_t (*callback)(void*, const char*, size_t),
        const char* format, va_list ap) {
    if (!callback) {
        callback = noop;
    }

    bool invalidConversion = false;
    int result = 0;
#define INCREMENT_RESULT(x) \
        if (__builtin_add_overflow(result, (x), &result)) goto overflow

    while (*format) {
loopBegin:
        if (*format != '%' || invalidConversion) {
            if (callback(param, format, 1) != 1) return -1;
            INCREMENT_RESULT(1);
        } else {
            const char* specifierBegin = format;

            int flags = 0;
            while (true) {
                switch (*++format) {
                case '\'': flags |= FLAG_THOUSANDS_GROUPING; continue;
                case '-': flags |= FLAG_LEFT_JUSTIFIED; continue;
                case '+': flags |= FLAG_PLUS; continue;
                case ' ': flags |= FLAG_SPACE; continue;
                case '#': flags |= FLAG_ALTERNATIVE; continue;
                case '0': flags |= FLAG_LEADING_ZEROS; continue;
                }
                break;
            }

            int fieldWidth = 0;
            if (*format == '*') {
                fieldWidth = va_arg(ap, int);
                if (fieldWidth < 0) {
                    flags |= FLAG_LEFT_JUSTIFIED;
                    fieldWidth = -fieldWidth;
                }
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    fieldWidth = fieldWidth * 10 + *format - '0';
                    format++;
                }
            }

            int precision = -1;
            if (*format == '.') {
                format++;
                if (*format == '*') {
                    precision = va_arg(ap, int);
                    format++;
                } else {
                    precision = 0;
                    while (*format >= '0' && *format <= '9') {
                        precision = precision * 10 + *format - '0';
                        format++;
                    }
                }
            }

            int length = LENGTH_INT;
            while (true) {
                switch (*format++) {
                // Parse length modifiers
                case 'h':
                    if (length == LENGTH_SHORT) {
                        length = LENGTH_CHAR;
                        break;
                    }
                    length = LENGTH_SHORT;
                    continue;
                case 'l':
                    if (length == LENGTH_LONG) {
                        length  = LENGTH_LONG_LONG;
                        break;
                    }
                    length = LENGTH_LONG;
                    continue;
                case 'j': length = LENGTH_INTMAX; break;
                case 'z': length = LENGTH_SIZE; break;
                case 't': length = LENGTH_PTRDIFF; break;
                case 'L': length = LENGTH_LONG_DOUBLE; break;
                case 'w': {
                    bool fast = false;
                    if (*format == 'f') {
                        fast = true;
                        format++;
                    }
                    size_t width = 0;
                    while (*format >= '0' && *format <= '9') {
                        width = width * 10 + *format - '0';
                        format++;
                    }
                    if (width != 8 && width != 16 && width != 32 &&
                            width != 64) {
                        invalidConversion = true;
                        format = specifierBegin;
                        goto loopBegin;
                    }

                    length = width + fast;
                } break;
                default:
                    format--;
                }
                break;
            }

            char specifier = *format;
            uintmax_t value = 0;
#define GET_ARG(type) (sizeof(type) >= sizeof(int) ? va_arg(ap, type) : \
        (type) va_arg(ap, int))

            if (specifier == 'b' || specifier == 'B' || specifier == 'o' ||
                    specifier == 'u' || specifier == 'x' || specifier == 'X') {
                value = length == LENGTH_CHAR ? GET_ARG(unsigned char) :
                        length == LENGTH_SHORT ? GET_ARG(unsigned short) :
                        length == LENGTH_INT ? GET_ARG(unsigned int) :
                        length == LENGTH_LONG ? GET_ARG(unsigned long) :
                        length == LENGTH_LONG_LONG ?
                        GET_ARG(unsigned long long) :
                        length == LENGTH_INTMAX ? GET_ARG(uintmax_t) :
                        length == LENGTH_SIZE ? GET_ARG(size_t) :
                        length == LENGTH_PTRDIFF ?
                        (uintmax_t) GET_ARG(ptrdiff_t) :
                        length == 8 ? GET_ARG(uint8_t) :
                        length == 16 ? GET_ARG(uint16_t) :
                        length == 32 ? GET_ARG(uint32_t) :
                        length == 64 ? GET_ARG(uint64_t) :
                        length == 8 + 1 ? GET_ARG(uint_fast8_t) :
                        length == 16 + 1 ? GET_ARG(uint_fast16_t) :
                        length == 32 + 1 ? GET_ARG(uint_fast32_t) :
                        length == 64 + 1 ? GET_ARG(uint_fast64_t) :
                        0;
            } else if (specifier == 'd' || specifier == 'i') {
                value = length == LENGTH_CHAR ? GET_ARG(signed char) :
                        length == LENGTH_SHORT ? GET_ARG(short) :
                        length == LENGTH_INT ? GET_ARG(int) :
                        length == LENGTH_LONG ? GET_ARG(long) :
                        length == LENGTH_LONG_LONG ? GET_ARG(long long) :
                        length == LENGTH_INTMAX ? GET_ARG(intmax_t) :
                        length == LENGTH_SIZE ? GET_ARG(ssize_t) :
                        length == LENGTH_PTRDIFF ? GET_ARG(ptrdiff_t) :
                        length == 8 ? GET_ARG(int8_t) :
                        length == 16 ? GET_ARG(int16_t) :
                        length == 32 ? GET_ARG(int32_t) :
                        length == 64 ? GET_ARG(int64_t) :
                        length == 8 + 1 ? GET_ARG(int_fast8_t) :
                        length == 16 + 1 ? GET_ARG(int_fast16_t) :
                        length == 32 + 1 ? GET_ARG(int_fast32_t) :
                        length == 64 + 1 ? GET_ARG(int_fast64_t) :
                        0;
            }

            size_t size;
            char c;
            const char* s;
            int written;

            switch (specifier) {
            case 'b': case 'B': case 'd': case 'i': case 'o': case 'u':
            case 'x': case 'X':
                if (precision < 0) {
                    precision = 1;
                } else {
                    flags &= ~FLAG_LEADING_ZEROS;
                }

                written = printInteger(param, callback, specifier, value,
                        fieldWidth, precision, flags);
                if (written < 0) return -1;
                INCREMENT_RESULT(written);
                break;
            case 'c':
                c = (char) va_arg(ap, int);
                written = printString(param, callback, &c, 1, fieldWidth,
                        flags);
                if (written < 0) return -1;
                INCREMENT_RESULT(written);
                break;
            case 's':
                s = va_arg(ap, const char*);

                if (precision < 0) {
                    size = strlen(s);
                } else {
                    size = strnlen(s, (size_t) precision);
                }
                if (size > INT_MAX) goto overflow;

                written = printString(param, callback, s, (int) size,
                        fieldWidth, flags);
                if (written < 0) return -1;
                INCREMENT_RESULT(written);
                break;
            case 'p':
                value = (uintptr_t) va_arg(ap, void*);
                written = printInteger(param, callback, 'x', value, 0, 1,
                        FLAG_ALTERNATIVE);
                if (written < 0) return -1;
                INCREMENT_RESULT(written);
                break;
            case 'n':
                length == LENGTH_CHAR ? *va_arg(ap, signed char*) = result :
                length == LENGTH_SHORT ? *va_arg(ap, short*) = result :
                length == LENGTH_INT ? *va_arg(ap, int*) = result :
                length == LENGTH_LONG ? *va_arg(ap, long*) = result :
                length == LENGTH_LONG_LONG ? *va_arg(ap, long long*) = result :
                length == LENGTH_INTMAX ? *va_arg(ap, intmax_t*) = result :
                length == LENGTH_SIZE ? *va_arg(ap, ssize_t*) = result :
                length == LENGTH_PTRDIFF ? *va_arg(ap, ptrdiff_t*) = result :
                length == 8 ? *va_arg(ap, int8_t*) = result :
                length == 16 ? *va_arg(ap, int16_t*) = result :
                length == 32 ? *va_arg(ap, int32_t*) = result :
                length == 64 ? *va_arg(ap, int64_t*) = result :
                length == 8 + 1 ? *va_arg(ap, int_fast8_t*) = result :
                length == 16 + 1 ? *va_arg(ap, int_fast16_t*) = result :
                length == 32 + 1 ? *va_arg(ap, int_fast32_t*) = result :
                length == 64 + 1 ? *va_arg(ap, int_fast64_t*) = result :
                        (void) 0;
                break;
            case '%':
                if (callback(param, "%", 1) != 1) return -1;
                INCREMENT_RESULT(1);
                break;
            default:
                // After finding an invalid (or unimplemented) conversion
                // specifier, we no longer know where the next argument begins,
                // so we will just print the remaining format string.
                invalidConversion = true;
                format = specifierBegin;
                continue;
            }
        }
        format++;
    }

    return result;

overflow:
    errno = EOVERFLOW;
    return -1;
}
__weak_alias(__vcbprintf, vcbprintf);

static int integerToString(char* output, uintmax_t value, unsigned int base,
        const char* digits) {
    // Calculate the length of the output string
    uintmax_t i = value;
    int length = 0;

    while (i > 0) {
        i /= base;
        length++;
    }

    output += length;

    do {
        *--output = digits[value % base];
        value /= base;
    } while (value);

    return length;
}

static size_t noop(void* param, const char* s, size_t nBytes){
    (void) param; (void) s;
    return nBytes;
}

static int printInteger(void* param,
        size_t (*callback)(void*, const char*, size_t), char specifier,
        uintmax_t value, int fieldWidth, int precision, int flags) {
    bool negative = false;

    if (specifier == 'd' || specifier == 'i') {
        intmax_t signedValue = (intmax_t) value;
        if (signedValue < 0) {
            negative = true;
            value = (uintmax_t) -signedValue;
        }
    } else {
        flags &= ~(FLAG_PLUS | FLAG_SPACE);
    }

    char buffer[(sizeof(uintmax_t) * CHAR_BIT)];
    const char* digits = specifier == 'X' ? upperDigits : lowerDigits;
    unsigned int base = 10;

    if (specifier == 'b' || specifier == 'B') {
        base = 2;
    } else if (specifier == 'o') {
        base = 8;
    } else if (specifier == 'x' || specifier == 'X') {
        base = 16;
    }

    int stringLength = integerToString(buffer, value, base, digits);

    if (flags & FLAG_ALTERNATIVE && specifier == 'o' &&
            stringLength >= precision) {
        if (__builtin_add_overflow(stringLength, 1, &precision)) {
            errno = EOVERFLOW;
            return -1;
        }
    }

    int unpaddedLength = stringLength >= precision ? stringLength : precision;
    if (negative || flags & (FLAG_PLUS | FLAG_SPACE)) {
        if (__builtin_add_overflow(unpaddedLength, 1, &unpaddedLength)) {
            errno = EOVERFLOW;
            return -1;
        }
    }
    if (flags & FLAG_ALTERNATIVE && value != 0 && (specifier == 'x' ||
            specifier == 'X' || specifier == 'b' || specifier == 'B')) {
        if (__builtin_add_overflow(unpaddedLength, 2, &unpaddedLength)) {
            errno = EOVERFLOW;
            return -1;
        }
    }

    if (!(flags & (FLAG_LEFT_JUSTIFIED | FLAG_LEADING_ZEROS))) {
        for (int i = unpaddedLength; i < fieldWidth; i++) {
            if (callback(param, " ", 1) != 1) return -1;
        }
    }

    if (negative || flags & (FLAG_PLUS | FLAG_SPACE)) {
        char sign = negative ? '-' : (flags & FLAG_PLUS) ? '+' : ' ';
        if (callback(param, &sign, 1) != 1) return -1;
    }

    if (flags & FLAG_ALTERNATIVE && value != 0) {
        if (specifier == 'x') {
            if (callback(param, "0x", 2) != 2) return -1;
        } else if (specifier == 'X') {
            if (callback(param, "0X", 2) != 2) return -1;
        } else if (specifier == 'b') {
            if (callback(param, "0b", 2) != 2) return -1;
        } else if (specifier == 'B') {
            if (callback(param, "0B", 2) != 2) return -1;
        }
    }

    if (!(flags & FLAG_LEFT_JUSTIFIED) && flags & FLAG_LEADING_ZEROS) {
        for (int i = unpaddedLength; i < fieldWidth; i++) {
            if (callback(param, "0", 1) != 1) return -1;
        }
    }

    for (int i = stringLength; i < precision; i++) {
        if (callback(param, "0", 1) != 1) return -1;
    }

    if (callback(param, buffer, stringLength) != (size_t) stringLength) {
        return -1;
    }

    if (flags & FLAG_LEFT_JUSTIFIED) {
        for (int i = unpaddedLength; i < fieldWidth; i++) {
            if (callback(param, " ", 1) != 1) return -1;
        }
    }

    return unpaddedLength >= fieldWidth ? unpaddedLength : fieldWidth;
}

static int printString(void* param,
        size_t (*callback)(void*, const char*, size_t), const char* s,
        int length, int fieldWidth, int flags) {
    if (!(flags & FLAG_LEFT_JUSTIFIED)) {
        for (int i = length; i < fieldWidth; i++) {
             if (callback(param, " ", 1) != 1) return -1;
        }
    }

    if (callback(param, s, length) != (size_t) length) return -1;

    if (flags & FLAG_LEFT_JUSTIFIED) {
        for (int i = length; i < fieldWidth; i++) {
            if (callback(param, " ", 1) != 1) return -1;
        }
    }

    return length >= fieldWidth ? length : fieldWidth;
}
