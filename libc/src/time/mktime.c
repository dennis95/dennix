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

/* libc/src/time/mktime.c
 * Converts broken-down local time to time_t. (C89)
 */

#define altzone __altzone
#define timegm __timegm
#define timezone __timezone
#define tzset __tzset
#include <errno.h>
#include <time.h>

time_t mktime(struct tm* tm) {
    tzset();
    time_t offset = tm->tm_isdst > 0 ? altzone : timezone;

    int oldErrno = errno;
    errno = 0;
    time_t result = timegm(tm);
    if (errno) return -1;

    errno = oldErrno;
    return result + offset;
}
