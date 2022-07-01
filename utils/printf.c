/* Copyright (c) 2022 Dennis Wölfing
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

/* utils/printf.c
 * Write formatted output.
 */

#include "utils.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>

static void print(const char* format, char* argv[]);
static size_t handleFormatSpecifier(const char* format, char* argv[]);
static long long getSigned(char* argv[]);
static unsigned long long getUnsigned(char* argv[]);
static const char* getString(char* argv[]);
static char* formatB(char* argv[]);

static int status = 0;
static bool stop = false;

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "FORMAT [ARGUMENTS...]\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case '?':
            return 1;
        }
    }

    if (optind >= argc) {
        errx(1, "missing format operand");
    }

    const char* format = argv[optind++];

    do {
        int optionIndex = optind;
        print(format, argv);
        if (optind == optionIndex) break;
    } while (optind < argc && !stop);

    return status;
}

static void print(const char* format, char* argv[]) {
    while (*format) {
        size_t literalLength = strcspn(format, "%\\");
        if (literalLength > 0) {
            fwrite(format, literalLength, 1, stdout);
            format += literalLength;
        }

        if (*format == '\\') {
            char c = '\0';
            format++;
            switch (*format++) {
            case '\\': c = '\\'; break;
            case 'a': c = '\a'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'v': c = '\v'; break;
            default:
                format--;
                if (*format < '0' || *format > '7') {
                    fputc('\\', stdout);
                    c = *format++;
                    break;
                }
                for (size_t i = 0; i < 3; i++) {
                    if (*format < '0' || *format > '7') break;
                    c = c * 8 + (*format - '0');
                    format++;
                }
            }

            fputc(c, stdout);
        }

        if (*format == '%') {
            format++;

            if (*format == '%') {
                fputc('%', stdout);
                format++;
            } else {
                format += handleFormatSpecifier(format, argv);
                if (stop) return;
            }
        }
    }
}

static size_t handleFormatSpecifier(const char* format, char* argv[]) {
    const char* conversionBegin = format;

    bool minusFlag = false;
    bool plusFlag = false;
    bool spaceFlag = false;
    bool alternateFlag = false;
    bool zeroFlag = false;

    while (true) {
        if (*format == '-') {
            minusFlag = true;
        } else if (*format == '+') {
            plusFlag = true;
        } else if (*format == ' ') {
            spaceFlag = true;
        } else if (*format == '#') {
            alternateFlag = true;
        } else if (*format == '0') {
            zeroFlag = true;
        } else {
            break;
        }
        format++;
    }

    int fieldWidth = 1;
    if (*format >= '0' && *format <= '9') {
        char* end;
        long value = strtol(format, &end, 10);
        if (value < 0 || value > INT_MAX) {
            err(1, "invalid field width");
        }
        format = end;
        fieldWidth = value;
    }

    int precision = -1;
    if (*format == '.') {
        format++;
        if (*format >= '0' && *format <= '9') {
            char* end;
            long value = strtol(format, &end, 10);
            if (value < 0 || value > INT_MAX) {
                err(1, "invalid precision");
            }
            format = end;
            precision = value;
        } else {
            precision = 0;
        }
    }

    char specifier = *format;

    // Build a conversion specification in a buffer. 13 bytes are enough.
    char buffer[13];
    char* spec = buffer;
    *spec++ = '%';
    if (specifier != 'c' && specifier != 's' && specifier != 'b') {
        if (minusFlag) {
            *spec++ = '-';
        }
        if (plusFlag) {
            *spec++ = '+';
        }
        if (spaceFlag) {
            *spec++ = ' ';
        }
        if (alternateFlag) {
            *spec++ = '#';
        }
        if (zeroFlag) {
            *spec++ = '0';
        }
    }

    *spec++ = '*';

    if (precision >= 0) {
        *spec++ = '.';
        *spec++ = '*';
    }

    if (specifier != 'c' && specifier != 's' && specifier != 'b') {
        *spec++ = 'l';
        *spec++ = 'l';
    }

    *spec++ = specifier == 'b' ? 's' : specifier;
    *spec = '\0';

    if (specifier == 'd' || specifier == 'i') {
        long long value = getSigned(argv);
        if (precision >= 0) {
            printf(buffer, fieldWidth, precision, value);
        } else {
            printf(buffer, fieldWidth, value);
        }
    } else if (specifier == 'o' || specifier == 'u' || specifier == 'x' ||
            specifier == 'X' || specifier == 'B') {
        unsigned long long value = getUnsigned(argv);
        if (precision >= 0) {
            printf(buffer, fieldWidth, precision, value);
        } else {
            printf(buffer, fieldWidth, value);
        }
    } else if (specifier == 'c') {
        printf(buffer, *getString(argv));
    } else if (specifier == 's') {
        const char* string = getString(argv);
        if (precision >= 0) {
            printf(buffer, fieldWidth, precision, string);
        } else {
            printf(buffer, fieldWidth, string);
        }
    } else if (specifier == 'b') {
        char* string = formatB(argv);
        if (precision >= 0) {
            printf(buffer, fieldWidth, precision, string);
        } else {
            printf(buffer, fieldWidth, string);
        }
        free(string);
    } else if (specifier == '\0') {
        errx(1, "missing format specifier");
    } else {
        errx(1, "invalid format specifier '%c'", specifier);
    }

    format++;
    return format - conversionBegin;
}

static long long getSigned(char* argv[]) {
    const char* argument = argv[optind];
    if (!argument) return 0;
    optind++;

    if (*argument == '\'' || *argument == '"') {
        size_t length = strlen(argument) - 1;
        if (length == 0) return 0;

        wchar_t wc;
        mbstate_t ps = {0};
        size_t converted = mbrtowc(&wc, argument + 1, length, &ps);
        if (converted > length) {
            warn("'%s'", argument);
            status = 1;
            wc = (unsigned char) argument[1];
        } else if (converted < length) {
            warnx("'%s': not completely converted", argument);
            status = 1;
        }

        return wc;
    }

    errno = 0;
    char* end;
    long long value = strtoll(argument, &end, 0);
    if (errno) {
        warn("'%s'", argument);
        status = 1;
    } else if (*end) {
        warnx("'%s': not completely converted", argument);
        status = 1;
    }
    return value;
}

static unsigned long long getUnsigned(char* argv[]) {
    const char* argument = argv[optind];
    if (!argument) return 0;
    optind++;

    if (*argument == '\'' || *argument == '"') {
        size_t length = strlen(argument) - 1;
        if (length == 0) return 0;

        wchar_t wc;
        mbstate_t ps = {0};
        size_t converted = mbrtowc(&wc, argument + 1, length, &ps);
        if (converted > length) {
            warn("'%s'", argument);
            status = 1;
            wc = (unsigned char) argument[1];
        } else if (converted < length) {
            warnx("'%s': not completely converted", argument);
            status = 1;
        }

        return wc;
    }

    errno = 0;
    char* end;
    unsigned long long value = strtoull(argument, &end, 0);
    if (errno) {
        warn("'%s'", argument);
        status = 1;
    } else if (*end) {
        warnx("'%s': not completely converted", argument);
        status = 1;
    }
    return value;
}

static const char* getString(char* argv[]) {
    const char* argument = argv[optind];
    if (!argument) return "";

    optind++;
    return argument;
}

static char* formatB(char* argv[]) {
    const char* input = getString(argv);

    char* buffer = malloc(strlen(input) + 1);
    if (!buffer) err(1, "malloc");

    char* str = buffer;

    while (*input) {
        if (*input == '\\') {
            input++;

            char c = '\0';
            switch (*input++) {
            case '\\': c = '\\'; break;
            case 'a': c = '\a'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'v': c = '\v'; break;

            case '0':
                for (size_t i = 0; i < 3; i++) {
                    if (*input < '0' || *input > '7') break;
                    c = c * 8 + (*input - '0');
                    input++;
                }
                break;
            case 'c':
                c = '\0';
                stop = true;
                break;
            default:
                *str++ = '\\';
                c = input[-1];
            }

            *str++ = c;
        } else {
            *str++ = *input++;
        }
    }

    *str = '\0';
    return buffer;
}
