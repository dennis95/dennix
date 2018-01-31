/* Copyright (c) 2018 Dennis Wölfing
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

/* libc/src/time/strftime.c
 * Converts time to a string.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

static const char* abday[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

static const char* day[] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

static const char* abmon[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

static const char* mon[] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

static int getFirstWeekDayOfYear(const struct tm* tm);
static time_t getWeekBasedYear(const struct tm* tm);
static ssize_t numberToString(char* buffer, time_t value);
static bool isLeapYear(time_t year);

static inline time_t time_abs(time_t value) {
    return value < 0 ? -value : value;
}

size_t strftime(char* restrict buffer, size_t size,
        const char* restrict format, const struct tm* restrict tm) {
    tzset();

    size_t index = 0;

    char defaultPadding;
    ssize_t fieldLength;
    ssize_t minLength;
    time_t number;
    bool numberAndFormat;
    char paddingChar;
    const char* string;
    const char* substitutedFormat;

#define PUTCHAR(c) \
    do { \
        if (index + 1 >= size) { \
            errno = ERANGE; \
            return 0; \
        } \
        buffer[index++] = (c); \
    } while (0)

    while (*format) {
        if (*format != '%') {
            PUTCHAR(*format++);
            continue;
        }

        defaultPadding = '0';
        fieldLength = 0;
        minLength = 0;
        numberAndFormat = false;
        paddingChar = '\0';

        bool plusModifier = false;

        switch (*++format) {
        case '0':
            paddingChar = '0';
            format++;
            break;
        case '+':
            paddingChar = '0';
            plusModifier = true;
            format++;
            break;
        case '_':
            paddingChar = ' ';
            format++;
            break;
        }

        while (*format >= '0' && *format <= '9') {
            fieldLength = fieldLength * 10 + *format - '0';
            format++;
        }

        switch (*format) {
        case 'E':
        case 'O':
            format++;
            break;
        }

        switch (*format) {
        case 'a':
            string = abday[tm->tm_wday % 7];
            goto putString;
        case 'A':
            string = day[tm->tm_wday % 7];
            goto putString;
        case 'b':
        case 'h':
            string = abmon[tm->tm_mon % 12];
            goto putString;
        case 'B':
            string = mon[tm->tm_mon % 12];
            goto putString;
        case 'c':
            substitutedFormat = "%a %b %e %H:%M:%S %Y";
            goto putFormat;
        case 'C':
            if (!fieldLength) fieldLength = 2;
            number = ((time_t) tm->tm_year + 1900) / 100;
            if (plusModifier && number >= 0 &&
                    (number >= 100 || fieldLength > 2)) {
                PUTCHAR('+');
                fieldLength--;
            }
            goto putNumber;
        case 'd':
            minLength = 2;
            number = tm->tm_mday;
            goto putNumber;
        case 'D':
            substitutedFormat = "%m/%d/%y";
            goto putFormat;
        case 'e':
            minLength = 2;
            defaultPadding = ' ';
            number = tm->tm_mday;
            goto putNumber;
        case 'F':
            if (!fieldLength) {
                fieldLength = 10;
                plusModifier = true;
            }

            fieldLength = fieldLength > 6 ? fieldLength - 6 : 0;

            number = (time_t) tm->tm_year + 1900;
            if (plusModifier && number >= 0 &&
                    (number >= 10000 || fieldLength > 4)) {
                PUTCHAR('+');
                if (fieldLength > 0) fieldLength--;
            }

            substitutedFormat = "-%m-%d";
            goto putNumberAndFormat;
        case 'g':
            minLength = 2;
            number = time_abs(getWeekBasedYear(tm) % 100);
            goto putNumber;
        case 'G':
            if (!fieldLength) fieldLength = 4;
            number = getWeekBasedYear(tm);
            if (plusModifier && number >= 0 &&
                    (number >= 10000 || fieldLength > 4)) {
                PUTCHAR('+');
                fieldLength--;
            }
            goto putNumber;
        case 'H':
            minLength = 2;
            number = tm->tm_hour;
            goto putNumber;
        case 'I':
            number = tm->tm_hour % 12;
            if (number == 0) number = 12;
            minLength = 2;
            goto putNumber;
        case 'j':
            minLength = 3;
            number = tm->tm_yday + 1;
            goto putNumber;
        case 'm':
            minLength = 2;
            number = tm->tm_mon + 1;
            goto putNumber;
        case 'M':
            minLength = 2;
            number = tm->tm_min;
            goto putNumber;
        case 'n':
            PUTCHAR('\n');
            break;
        case 'p':
            string = tm->tm_hour < 12 ? "AM" : "PM";
            goto putString;
        case 'r':
            substitutedFormat = "%I:%M:%S %p";
            goto putFormat;
        case 'R':
            substitutedFormat = "%H:%M";
            goto putFormat;
        case 's': {
            struct tm tmCopy = *tm;
            number = mktime(&tmCopy);
            goto putNumber;
        }
        case 'S':
            minLength = 2;
            number = tm->tm_sec;
            goto putNumber;
        case 't':
            PUTCHAR('\t');
            break;
        case 'T':
            substitutedFormat = "%H:%M:%S";
            goto putFormat;
        case 'u':
            number = tm->tm_wday ? tm->tm_wday : 7;
            goto putNumber;
        case 'U':
            number = (tm->tm_yday + 7 -
                    (7 - getFirstWeekDayOfYear(tm)) % 7) / 7;
            minLength = 2;
            goto putNumber;
        case 'V': {
            int firstWday = getFirstWeekDayOfYear(tm);

            if (firstWday == 0 || firstWday >= 5) {
                if (tm->tm_yday - (8 - firstWday) % 7 >= 0) {
                    number = (tm->tm_yday + 7 - (8 - firstWday) % 7) / 7;
                } else if (firstWday == 0) {
                    number = 52;
                } else if (firstWday == 5) {
                    number = 53;
                } else /*if (firstWday == 6)*/ {
                    number = isLeapYear((time_t) tm->tm_year + 1900 - 1) ?
                            53 : 52;
                }
            } else {
                number = (tm->tm_yday + 14 - (8 - firstWday) % 7) / 7;
            }

            minLength = 2;
            goto putNumber;
        }
        case 'w':
            number = tm->tm_wday;
            goto putNumber;
        case 'W':
            number = (tm->tm_yday + 7 -
                    (8 - getFirstWeekDayOfYear(tm)) % 7) / 7;
            minLength = 2;
            goto putNumber;
        case 'x':
            substitutedFormat = "%m/%d/%y";
            goto putFormat;
        case 'X':
            substitutedFormat = "%H:%M:%S";
            goto putFormat;
        case 'y':
            minLength = 2;
            number = time_abs(tm->tm_year % 100);
            goto putNumber;
        case 'Y':
            if (!fieldLength) {
                fieldLength = 4;
                plusModifier = true;
            }
            number = (time_t) tm->tm_year + 1900;
            if (plusModifier && number >= 0 &&
                    (number >= 10000 || fieldLength > 4)) {
                PUTCHAR('+');
                fieldLength--;
            }
            goto putNumber;
        case 'z': {
            int minutes = (tm->tm_isdst ? altzone : timezone) / 60;
            int hours = minutes / 60;
            minutes = time_abs(minutes % 60);

            if (hours > 0) {
                PUTCHAR('-');
            } else {
                PUTCHAR('+');
                hours = -hours;
            }

            number = hours * 100 + minutes;
            minLength = 4;
            goto putNumber;
        }
        case 'Z':
            if (tm->tm_isdst < 0) break;
            string = tm->tm_isdst ? tzname[1] : tzname[0];
            goto putString;
        case '%':
            PUTCHAR('%');
            break;
        default:
            PUTCHAR('%');
            continue;
        }

velociraptor: // https://xkcd.com/292
        format++;
    }

    buffer[index] = '\0';
    return index;

putNumberAndFormat:
    numberAndFormat = true;
putNumber:
    if (!paddingChar) paddingChar = defaultPadding;

    // This buffer is large enough to contain any time_t value.
    char numberBuffer[sizeof(time_t) * 3];
    char* numberBufferEnd = numberBuffer + sizeof(numberBuffer) - 1;

    ssize_t stringLength = numberToString(numberBufferEnd, number);
    if (fieldLength < minLength) {
        fieldLength = minLength;
    }
    ssize_t paddingLength = fieldLength - stringLength;
    if (number < 0) {
        PUTCHAR('-');
        paddingLength--;
    }
    for (ssize_t i = 0; i < paddingLength; i++) {
        PUTCHAR(paddingChar);
    }
    string = numberBufferEnd - stringLength;

putString:
    while (*string) {
        PUTCHAR(*string++);
    }
    if (!numberAndFormat) goto velociraptor;

putFormat:
    {
        int oldErrno = errno;
        errno = 0;
        size_t length = strftime(buffer + index, size - index,
                substitutedFormat, tm);
        if (!length && errno) {
            return 0;
        }
        errno = oldErrno;
        index += length;
        goto velociraptor;
    }
}

static int getFirstWeekDayOfYear(const struct tm* tm) {
    int wday = (tm->tm_wday - tm->tm_yday) % 7;
    return wday < 0 ? wday + 7 : wday;
}

static time_t getWeekBasedYear(const struct tm* tm) {
    int firstWeekDay = getFirstWeekDayOfYear(tm);
    time_t year = (time_t) tm->tm_year + 1900;
    if (firstWeekDay == 0 || firstWeekDay >= 5) {
        year--;
    }
    return year;
}

static ssize_t numberToString(char* buffer, time_t value) {
    value = time_abs(value);
    ssize_t index = 0;
    buffer[0] = '\0';
    do {
        index++;
        buffer[-index] = "0123456789"[value % 10];
        value /= 10;
    } while (value);

    return index;
}

static bool isLeapYear(time_t year) {
    return !(year % 4) && ((year % 100) || !(year % 400));
}
