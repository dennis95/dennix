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

/* libc/src/time/timegm.c
 * Converts broken down time to time_t. (called from C89)
 */

#include <errno.h>
#include <stdbool.h>
#include <time.h>

static bool isLeapYear(time_t year) {
    return !(year % 4) && ((year % 100) || !(year % 400));
}

static time_t daysPerYear(time_t year) {
    return isLeapYear(year) ? 366 : 365;
}

enum {
    JANUARY = 0,
    FEBRUARY,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER
};

static time_t daysPerMonth(int month, time_t year) {
    switch (month) {
    case JANUARY: return 31;
    case FEBRUARY: return isLeapYear(year) ? 29 : 28;
    case MARCH: return 31;
    case APRIL: return 30;
    case MAY: return 31;
    case JUNE: return 30;
    case JULY: return 31;
    case AUGUST: return 31;
    case SEPTEMBER: return 30;
    case OCTOBER: return 31;
    case NOVEMBER: return 30;
    case DECEMBER: return 31;
    default: __builtin_unreachable();
    }
}

static bool normalize(int* x, int* y, int range) {
    if (__builtin_add_overflow(*y, *x / range, y)) return false;

    *x %= range;
    if (*x < 0) {
        *x += range;
        (*y)--;
    }

    return true;
}

static bool normalizeEntries(struct tm* tm) {
    if (!normalize(&tm->tm_sec, &tm->tm_min, 60) ||
            !normalize(&tm->tm_min, &tm->tm_hour, 60) ||
            !normalize(&tm->tm_hour, &tm->tm_mday, 24)) {
        return false;
    }

    // We cannot normalize tm_mday the same way because not all months have the
    // same amount of days.

    if (!normalize(&tm->tm_mon, &tm->tm_year, 12)) return false;

    // Now normalize tm_mday.
    while (tm->tm_mday > daysPerMonth(tm->tm_mon,
            (time_t) tm->tm_year + 1900)) {
        tm->tm_mday -= daysPerMonth(tm->tm_mon, (time_t) tm->tm_year + 1900);
        tm->tm_mon++;
        if (tm->tm_mon > DECEMBER) {
            if (__builtin_add_overflow(tm->tm_year, 1, &tm->tm_year)) {
                return false;
            }
            tm->tm_mon = JANUARY;
        }
    }
    while (tm->tm_mday <= 0) {
        tm->tm_mon--;
        if (tm->tm_mon < JANUARY) {
            if (__builtin_sub_overflow(tm->tm_year, 1, &tm->tm_year)) {
                return false;
            }
            tm->tm_mon = DECEMBER;
        }
        tm->tm_mday += daysPerMonth(tm->tm_mon, (time_t) tm->tm_year + 1900);
    }

    return true;
}

time_t __timegm(struct tm* tm) {
    // The values in the tm structure might be outside of their usual range.
    // We need to normalize them before we can use them.
    if (!normalizeEntries(tm)) {
        errno = EOVERFLOW;
        return -1;
    }

    time_t year = 1970;
    time_t daysSinceEpoch = 0;

    while (year < 1900 + (time_t) tm->tm_year) {
        daysSinceEpoch += daysPerYear(year);
        year++;
    }

    while (year > 1900 + (time_t) tm->tm_year) {
        year--;
        daysSinceEpoch -= daysPerYear(year);
    }

    int month = JANUARY;
    tm->tm_yday = 0;

    while (month < tm->tm_mon) {
        daysSinceEpoch += daysPerMonth(month, year);
        tm->tm_yday += daysPerMonth(month, year);
        month++;
    }

    daysSinceEpoch += tm->tm_mday - 1;

    // Since time_t is 64 bit and int is only 32 bit, no struct tm can cause
    // time_t to overflow.
    time_t secondsSinceEpoch = daysSinceEpoch * 24 * 60 * 60;
    tm->tm_yday += tm->tm_mday - 1;

    secondsSinceEpoch += tm->tm_hour * 60 * 60;
    secondsSinceEpoch += tm->tm_min * 60;
    secondsSinceEpoch += tm->tm_sec;

    tm->tm_wday = (4 + daysSinceEpoch) % 7;
    if (tm->tm_wday < 0) {
        tm->tm_wday += 7;
    }

    return secondsSinceEpoch;
}
__weak_alias(__timegm, timegm);
