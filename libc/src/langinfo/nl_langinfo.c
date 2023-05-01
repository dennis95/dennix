/* Copyright (c) 2020, 2023 Dennis Wölfing
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

/* libc/src/langinfo/nl_langinfo.c
 * Language and locale information. (POSIX2008)
 */

#include <langinfo.h>

char* nl_langinfo(nl_item item) {
    switch (item) {
    case CODESET: return "UTF-8";
    case D_T_FMT: return "%a %b %e %H:%M:%S %Y";
    case D_FMT: return "%m/%d/%y";
    case T_FMT: return "%H:%M:%S";
    case T_FMT_AMPM: return "%I:%M:%S %p";
    case AM_STR: return "AM";
    case PM_STR: return "PM";
    case DAY_1: return "Sunday";
    case DAY_2: return "Monday";
    case DAY_3: return "Tuesday";
    case DAY_4: return "Wednesday";
    case DAY_5: return "Thursday";
    case DAY_6: return "Friday";
    case DAY_7: return "Saturday";
    case ABDAY_1: return "Sun";
    case ABDAY_2: return "Mon";
    case ABDAY_3: return "Tue";
    case ABDAY_4: return "Wed";
    case ABDAY_5: return "Thu";
    case ABDAY_6: return "Fri";
    case ABDAY_7: return "Sat";
    case MON_1: case ALTMON_1: return "January";
    case MON_2: case ALTMON_2: return "February";
    case MON_3: case ALTMON_3: return "March";
    case MON_4: case ALTMON_4: return "April";
    case MON_5: case ALTMON_5: return "May";
    case MON_6: case ALTMON_6: return "June";
    case MON_7: case ALTMON_7: return "July";
    case MON_8: case ALTMON_8: return "August";
    case MON_9: case ALTMON_9: return "September";
    case MON_10: case ALTMON_10: return "October";
    case MON_11: case ALTMON_11: return "November";
    case MON_12: case ALTMON_12: return "December";
    case ABMON_1: case ABALTMON_1: return "Jan";
    case ABMON_2: case ABALTMON_2: return "Feb";
    case ABMON_3: case ABALTMON_3: return "Mar";
    case ABMON_4: case ABALTMON_4: return "Apr";
    case ABMON_5: case ABALTMON_5: return "May";
    case ABMON_6: case ABALTMON_6: return "Jun";
    case ABMON_7: case ABALTMON_7: return "Jul";
    case ABMON_8: case ABALTMON_8: return "Aug";
    case ABMON_9: case ABALTMON_9: return "Sep";
    case ABMON_10: case ABALTMON_10: return "Oct";
    case ABMON_11: case ABALTMON_11: return "Nov";
    case ABMON_12: case ABALTMON_12: return "Dec";
    case ERA: case ERA_D_FMT: case ERA_D_T_FMT: case ERA_T_FMT: return "";
    case ALT_DIGITS: return "";
    case RADIXCHAR: return ".";
    case THOUSEP: return "";
    case YESEXPR: return "^[yY]";
    case NOEXPR: return "^[nN]";
    case CRNCYSTR: return "";
    default: return "";
    }
}
