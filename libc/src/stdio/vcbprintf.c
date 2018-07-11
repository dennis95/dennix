/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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
 * Print formatted output.
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
    LENGTH_CHAR,
    LENGTH_SHORT,
    LENGTH_INT,
    LENGTH_LONG,
    LENGTH_LONG_LONG,
    LENGTH_INTMAX,
    LENGTH_SIZE,
    LENGTH_PTRDIFF,
    LENGTH_LONG_DOUBLE
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

int vcbprintf(void* param, size_t (*callback)(void*, const char*, size_t),
        const char* format, va_list ap) {
    if (!callback) {
        callback = noop;
    }

    bool invalidConversion = false;
    int result = 0;
#define INCREMENT_RESULT(x) \
        if (__builtin_add_overflow(result, (x), &result)) goto overflow

    while (*format) {
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
                case 'j':
                    length = LENGTH_INTMAX;
                    break;
                case 'z':
                    length = LENGTH_SIZE;
                    break;
                case 't':
                    length = LENGTH_PTRDIFF;
                    break;
                case 'L':
                    length = LENGTH_LONG_DOUBLE;
                    break;
                default:
                    format--;
                }
                break;
            }

            char specifier = *format;
            uintmax_t value = 0;

            if (specifier == 'o' || specifier == 'u' || specifier == 'x' ||
                    specifier == 'X') {
                switch (length) {
                case LENGTH_CHAR:
                    value = (unsigned char) va_arg(ap, unsigned int);
                    break;
                case LENGTH_SHORT:
                    value = (unsigned short) va_arg(ap, unsigned int);
                    break;
                case LENGTH_INT:
                    value = va_arg(ap, unsigned int);
                    break;
                case LENGTH_LONG:
                    value = va_arg(ap, unsigned long);
                    break;
                case LENGTH_LONG_LONG:
                    value = va_arg(ap, unsigned long long);
                    break;
                case LENGTH_INTMAX:
                    value = va_arg(ap, uintmax_t);
                    break;
                case LENGTH_SIZE:
                    value = va_arg(ap, size_t);
                    break;
                case LENGTH_PTRDIFF:
                    value = va_arg(ap, ptrdiff_t);
                    break;
                }
            } else if (specifier == 'd' || specifier == 'i') {
                intmax_t signedValue = 0;
                switch (length) {
                case LENGTH_CHAR:
                    signedValue = (signed char) va_arg(ap, int);
                    break;
                case LENGTH_SHORT:
                    signedValue = (short) va_arg(ap, int);
                    break;
                case LENGTH_INT:
                    signedValue = va_arg(ap, int);
                    break;
                case LENGTH_LONG:
                    signedValue = va_arg(ap, long);
                    break;
                case LENGTH_LONG_LONG:
                    signedValue = va_arg(ap, long long);
                    break;
                case LENGTH_INTMAX:
                    signedValue = va_arg(ap, intmax_t);
                    break;
                case LENGTH_SIZE:
                    signedValue = va_arg(ap, ssize_t);
                    break;
                case LENGTH_PTRDIFF:
                    signedValue = va_arg(ap, ptrdiff_t);
                    break;
                }

                value = (uintmax_t) signedValue;
            }

            size_t size;
            char c;
            const char* s;
            int written;

            switch (specifier) {
            case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
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
                *va_arg(ap, int*) = result;
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

    char buffer[(sizeof(uintmax_t) * 3)];
    const char* digits = specifier == 'X' ? upperDigits : lowerDigits;
    unsigned int base = 10;

    if (specifier == 'o') {
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
    if (flags & FLAG_ALTERNATIVE && (specifier == 'x' || specifier == 'X') &&
            value != 0) {
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

    if (flags & FLAG_ALTERNATIVE) {
        if (specifier == 'x' && value != 0) {
            if (callback(param, "0x", 2) != 2) return -1;
        } else if (specifier == 'X' && value != 0) {
            if (callback(param, "0X", 2) != 2) return -1;
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
