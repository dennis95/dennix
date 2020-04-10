/* Copyright (c) 2020 Dennis Wölfing
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

/* utils/touch.c
 * Update file timestamps.
 */

#include "utils.h"
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static bool parseDate(const char* str, struct timespec* ts);
static bool parseTime(const char* str, struct timespec* ts);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "date", required_argument, 0, 'd' },
        { "no-create", no_argument, 0, 'c' },
        { "reference", required_argument, 0, 'r' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool accessTime = false;
    bool modificationTime = false;
    bool noCreate = false;
    char* dateString = NULL;
    char* referenceFile = NULL;
    char* timeString = NULL;
    int c;
    while ((c = getopt_long(argc, argv, "acd:mr:t:", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] FILE...\n"
                    "  -a                       change access time\n"
                    "  -c, --no-create          do not create new files\n"
                    "  -d, --date=DATE          use DATE\n"
                    "  -m                       change modification time\n"
                    "  -r, --reference=FILE     use FILE's timestamps\n"
                    "  -t TIME                  use TIME\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'a':
            accessTime = true;
            break;
        case 'c':
            noCreate = true;
            break;
        case 'd':
            dateString = optarg;
            break;
        case 'm':
            modificationTime = true;
            break;
        case 'r':
            referenceFile = optarg;
            break;
        case 't':
            timeString = optarg;
            break;
        }
    }

    if (!accessTime && !modificationTime) {
        accessTime = true;
        modificationTime = true;
    }

    struct timespec ts[2];
    ts[0].tv_nsec = UTIME_NOW;
    ts[1].tv_nsec = UTIME_NOW;

    if (dateString) {
        if (referenceFile || timeString) {
            errx(1, "multiple time sources specified");
        }
        if (!parseDate(dateString, &ts[0])) {
            errx(1, "invalid date format: '%s'", dateString);
        }
        ts[1] = ts[0];
    }

    if (referenceFile) {
        if (timeString) errx(1, "multiple time sources specified");
        struct stat st;
        if (stat(referenceFile, &st) < 0) err(1, "stat: '%s'", referenceFile);
        ts[0] = st.st_atim;
        ts[1] = st.st_mtim;
    }

    if (timeString) {
        if (!parseTime(timeString, &ts[0])) {
            errx(1, "invalid date format: '%s'", timeString);
        }
        ts[1] = ts[0];
    }

    if (!accessTime) {
        ts[0].tv_nsec = UTIME_OMIT;
    }
    if (!modificationTime) {
        ts[1].tv_nsec = UTIME_OMIT;
    }
    if (optind >= argc) errx(1, "missing operand");

    int status = 0;
    for (int i = optind; i < argc; i++) {
        if (access(argv[i], F_OK) == 0) {
            if (utimensat(AT_FDCWD, argv[i], ts, 0) < 0) {
                warn("utimensat: '%s'", argv[i]);
                status = 1;
            }
        } else if (!noCreate) {
            int fd = open(argv[i], O_WRONLY | O_CREAT, 0666);
            if (fd < 0) {
                warn("open: '%s'", argv[i]);
                status = 1;
                continue;
            }
            if (futimens(fd, ts) < 0) {
                warn("futimens: '%s'", argv[i]);
                status = 1;
            }
            close(fd);
        }
    }

    return status;
}

static bool parseDate(const char* str, struct timespec* ts) {
    struct tm tm;
    char* end;
    long long year = strtoll(str, &end, 10);
    if (year < INT_MIN + 1900 || year - 1900 > INT_MAX) {
        return false;
    }
    tm.tm_year = year - 1900;
    if (strlen(end) < 15) return false;

    if (end[0] != '-' || end[3] != '-' || (end[6] != 'T' && end[6] != ' ') ||
            end[9] != ':' || end[12] != ':') {
        return false;
    }
    for (size_t i = 1; i < 15; i++) {
        if (i % 3 == 0) continue;
        if (!isdigit(end[i])) return false;
    }

    tm.tm_mon = (end[1] - '0') * 10 + end[2] - '0' - 1;
    if (tm.tm_mon < 0 || tm.tm_mon >= 12) return false;
    tm.tm_mday = (end[4] - '0') * 10 + end[5] - '0';
    if (tm.tm_mday == 0 || tm.tm_mday > 31) return false;
    tm.tm_hour = (end[7] - '0') * 10 + end[8] - '0';
    if (tm.tm_hour > 23) return false;
    tm.tm_min = (end[10] - '0') * 10 + end[11] - '0';
    if (tm.tm_min > 59) return false;
    tm.tm_sec = (end[13] - '0') * 10 + end[14] - '0';
    if (tm.tm_sec > 60) return false;

    end += 15;
    ts->tv_nsec = 0;
    if (end[0] == '.' || end[0] == ',') {
        char* endOfDigits;
        ts->tv_nsec = strtol(end + 1, &endOfDigits, 10);
        size_t digits = endOfDigits - end - 1;
        while (digits < 9) {
            ts->tv_nsec *= 10;
            digits++;
        }
        while (digits > 9) {
            ts->tv_nsec /= 10;
            digits--;
        }
        if (ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000) return false;
        end = endOfDigits;
    }
    if (end[0] == 'Z' && end[1] == '\0') {
        ts->tv_sec = timegm(&tm);
    } else {
        if (end[0] != '\0') return false;
        ts->tv_sec = mktime(&tm);
    }

    return true;
}

static bool parseTime(const char* str, struct timespec* ts) {
    size_t length = strlen(str);
    if (length < 8 || length == 9 || length == 14 || length > 15) return false;

    for (size_t i = 0; i < length; i++) {
        if (length % 2 && i == length - 3) {
            if (str[i] != '.') return false;
            continue;
        }
        if (!isdigit(str[i])) return false;
    }

    struct tm tm;
    tm.tm_sec = 0;
    if (length % 2) {
        if (str[length - 3] != '.') return false;
        tm.tm_sec = (str[length - 2] - '0') * 10 + str[length - 1] - '0';
        if (tm.tm_sec > 60) return false;
        length -= 3;
    }

    tm.tm_min = (str[length - 2] - '0') * 10 + str[length - 1] - '0';
    if (tm.tm_min > 59) return false;
    tm.tm_hour = (str[length - 4] - '0') * 10 + str[length - 3] - '0';
    if (tm.tm_hour > 23) return false;
    tm.tm_mday = (str[length - 6] - '0') * 10 + str[length - 5] - '0';
    if (tm.tm_mday == 0 || tm.tm_mday > 31) return false;
    tm.tm_mon = (str[length - 8] - '0') * 10 + str[length - 7] - '0' - 1;
    if (tm.tm_mon < 0 || tm.tm_mon >= 12) return false;

    int year;
    if (length >= 10) {
        year = (str[length - 10] - '0') * 10 + str[length - 9] - '0';
        if (length == 12) {
            year += (str[0] - '0') * 1000 + (str[1] - '0') * 100;
        } else if (year >= 69) {
            year += 1900;
        } else {
            year += 2000;
        }
    } else {
        time_t t = time(NULL);
        struct tm* now = localtime(&t);
        if (!now) err(1, "localtime");
        year = now->tm_year;
    }
    tm.tm_year = year - 1900;
    ts->tv_sec = mktime(&tm);
    ts->tv_nsec = 0;
    return true;
}
