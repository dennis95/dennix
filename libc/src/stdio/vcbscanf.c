/* Copyright (c) 2018, 2022 Dennis Wölfing
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

/* libc/src/stdio/vcbscanf.c
 * Scan formatted input. (called from C89)
 */

#define reallocarray __reallocarray
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: Support numbered arguments

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

static unsigned int getDigitValue(int c);
static void storeInt(va_list* ap, int length, bool sign, uintmax_t value);

int __vcbscanf(void* arg, int (*get)(void*), int (*unget)(int, void*),
        const char* restrict format, va_list list) {
    size_t bytesRead = 0;
    int conversions = 0;
    int c;

    // Create a copy to work around va_list being an array type on some
    // architectures.
    va_list ap;
    va_copy(ap, list);

#define GET() ({ int _c = get(arg); if (_c != EOF) bytesRead++; _c; })
#define UNGET(c) ((c) == EOF ? EOF : (bytesRead--, unget((c), arg)))

    while (*format) {
        if (isspace(*format)) {
            c = GET();
            while (isspace(c)) {
                c = GET();
            }
            UNGET(c);
        } else if (*format != '%') {
            c = GET();
            if (c != (unsigned char) *format) {
                UNGET(c);
                goto fail;
            }
        } else {
            format++;

            bool noAssignment = false;
            if (*format == '*') {
                noAssignment = true;
                format++;
            }

            size_t fieldWidth = 0;
            while (*format >= '0' && *format <= '9') {
                fieldWidth = fieldWidth * 10 + *format - '0';
                format++;
            }

            bool allocate = false;
            if (*format == 'm') {
                allocate = true;
                format++;
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

            switch (*format) {
            case 'd':
            case 'i':
            case 'o':
            case 'u':
            case 'x': case 'X':
            case 'p': {
                c = GET();
                while (isspace(c)) {
                    c = GET();
                }

                bool negative = false;

                if (fieldWidth == 0) {
                    fieldWidth = SIZE_MAX;
                }

                if (c == EOF) {
                    goto fail;
                } else if (c == '+') {
                    c = GET();
                    fieldWidth--;
                } else if (c == '-') {
                    negative = true;
                    c = GET();
                    fieldWidth--;
                }

                if (!fieldWidth) {
                    UNGET(c);
                    va_end(ap);
                    return conversions;
                }

                unsigned int base = *format == 'd' ? 10 :
                        *format == 'i' ? 0 :
                        *format == 'o' ? 8 :
                        *format == 'u' ? 10 : 16;

                bool sign = *format == 'd' || *format == 'i';

                if (base == 0) {
                    if (c == '0') {
                        c = GET();
                        if (fieldWidth > 1 && (c == 'x' || c == 'X')) {
                            fieldWidth -= 2;
                            if (!fieldWidth) {
                                va_end(ap);
                                return conversions;
                            }
                            base = 16;
                            c = GET();
                        } else {
                            UNGET(c);
                            c = '0';
                            base = 8;
                        }
                    } else {
                        base = 10;
                    }
                } else if (base == 16 && c == '0') {
                    c = GET();
                    if (fieldWidth > 1 && (c == 'x' || c == 'X')) {
                        fieldWidth -= 2;
                        if (!fieldWidth) {
                            va_end(ap);
                            return conversions;
                        }
                        c = GET();
                    } else {
                        UNGET(c);
                        c = '0';
                    }
                }

                if (getDigitValue(c) >= base) {
                    UNGET(c);
                    va_end(ap);
                    return conversions;
                }

                uintmax_t value = 0;

                for (size_t i = 0; i < fieldWidth; i++) {
                    unsigned int digit = getDigitValue(c);
                    if (digit >= base) break;
                    value = value * base + digit;
                    c = GET();
                }
                UNGET(c);

                if (negative) {
                    value = -value;
                }

                if (!noAssignment) {
                    if (*format == 'p') {
                        *va_arg(ap, void**) = (void*) (uintptr_t) value;
                    } else {
                        storeInt(&ap, length, sign, value);
                    }
                    conversions++;
                }
            } break;

            case 's':
            case 'c': {
                c = GET();
                if (*format == 's') {
                    while (isspace(c)) {
                        c = GET();
                    }
                }

                if (c == EOF) goto fail;

                if (!fieldWidth) {
                    fieldWidth = *format == 'c' ? 1 : SIZE_MAX;
                }

                char* s;
                size_t allocatedSize = 0;
                if (allocate) {
                    allocatedSize = *format == 'c' ? fieldWidth : 80;
                    s = malloc(allocatedSize);
                    if (!s) goto conversion_error;
                } else {
                    s = va_arg(ap, char*);
                }

                size_t i;
                for (i = 0; i < fieldWidth; i++) {
                    if (c == EOF || (*format == 's' && isspace(c))) {
                        if (*format == 'c') {
                            if (allocate) {
                                free(s);
                            }
                            va_end(ap);
                            return conversions;
                        }
                        break;
                    }
                    s[i] = c;
                    if (allocate && *format != 'c' && i >= allocatedSize - 1) {
                        char* newBuffer = reallocarray(s, 2, allocatedSize);
                        if (!newBuffer) {
                            free(s);
                            goto conversion_error;
                        }
                        s = newBuffer;
                        allocatedSize *= 2;
                    }
                    c = GET();
                }
                UNGET(c);
                if (*format != 'c') {
                    s[i] = '\0';
                }

                if (allocate) {
                    *va_arg(ap, char**) = s;
                }

                conversions++;
            } break;
            case 'n':
                storeInt(&ap, length, true, bytesRead);
                break;
            case '%':
                c = GET();
                if (c != '%') {
                    UNGET(c);
                    goto fail;
                }
                break;

            case '[':
                // TODO: Implement %[
            default:
                // Unsupported conversion.
                va_end(ap);
                return conversions;
            }
        }

        format++;
    }

    va_end(ap);
    return conversions;
conversion_error:
    c = EOF;
fail:
    va_end(ap);
    if (!conversions && c == EOF) return EOF;
    return conversions;
}
__weak_alias(__vcbscanf, vcbscanf);

static unsigned int getDigitValue(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    } else if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    } else {
        return -1;
    }
}

static void storeInt(va_list* ap, int length, bool sign, uintmax_t value) {
    if (sign) {
        switch (length) {
        case LENGTH_CHAR:
            *va_arg(*ap, signed char*) = value;
            break;
        case LENGTH_SHORT:
            *va_arg(*ap, short*) = value;
            break;
        case LENGTH_INT:
            *va_arg(*ap, int*) = value;
            break;
        case LENGTH_LONG:
            *va_arg(*ap, long*) = value;
            break;
        case LENGTH_LONG_LONG:
            *va_arg(*ap, long long*) = value;
            break;
        case LENGTH_INTMAX:
            *va_arg(*ap, intmax_t*) = value;
            break;
        case LENGTH_SIZE:
            *va_arg(*ap, ssize_t*) = value;
            break;
        case LENGTH_PTRDIFF:
            *va_arg(*ap, ptrdiff_t*) = value;
            break;
        }
    } else {
        switch (length) {
        case LENGTH_CHAR:
            *va_arg(*ap, unsigned char*) = value;
            break;
        case LENGTH_SHORT:
            *va_arg(*ap, unsigned short*) = value;
            break;
        case LENGTH_INT:
            *va_arg(*ap, unsigned int*) = value;
            break;
        case LENGTH_LONG:
            *va_arg(*ap, unsigned long*) = value;
            break;
        case LENGTH_LONG_LONG:
            *va_arg(*ap, unsigned long long*) = value;
            break;
        case LENGTH_INTMAX:
            *va_arg(*ap, uintmax_t*) = value;
            break;
        case LENGTH_SIZE:
            *va_arg(*ap, size_t*) = value;
            break;
        case LENGTH_PTRDIFF:
            *va_arg(*ap, ptrdiff_t*) = value;
            break;
        }
    }
}
