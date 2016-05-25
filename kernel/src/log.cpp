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

/* kernel/src/log.h
 * Defines functions to print to the screen.
 */

#include <stdarg.h>
#include <stddef.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/terminal.h>

static void printCharacter(char c);
static void printString(const char* s);
static void printNumber(unsigned u, int base);

void Log::printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);

    unsigned u;
    unsigned char c;
    const char* s;

    while (*format) {
        if (*format != '%') {
            printCharacter(*format);
        } else {
            switch (*++format) {
            case 'u':
                u = va_arg(ap, unsigned);
                printNumber(u, 10);
                break;
            case 'x':
                u = va_arg(ap, unsigned);
                printNumber(u, 16);
                break;
            case 'c':
                c = (unsigned char) va_arg(ap, int);
                printCharacter(c);
                break;
            case 's':
                s = va_arg(ap, char*);
                printString(s);
                break;
            case '%':
                printCharacter('%');
                break;
            case '\0':
                // The format string ended prematurely.
                va_end(ap);
                return;
            default:
                // Unknown conversion specifier
                // TODO: implement more specifiers
                printCharacter('%');
                printCharacter(*format);
            }
        }

        format++;
    }
    va_end(ap);
}

static void printCharacter(char c) {
    terminal.write(&c, 1);
}

static void printString(const char* s) {
    while (*s) {
        printCharacter(*s++);
    }
}

static void printNumber(unsigned u, int base) {
    static const char* digits = "0123456789abcdef";

    // This buffer is big enough to contain any 32 bit number.
    char buffer[11];
    char* p = buffer + 10;
    *p = '\0';

    do {
        *--p = digits[u % base];
        u /= base;
    } while (u);

    printString(p);
}
