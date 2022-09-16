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

/* libc/src/signal/str2sig.c
 * Translate a signal name to a signal number.
 */

#include <signal.h>
#include <stdlib.h>
#include <string.h>

extern const char* const __signalnames[NSIG];

int str2sig(const char* restrict str, int* restrict num) {
    if (*str >= '0' && *str <= '9') {
        char* end;
        long n = strtol(str, &end, 10);
        if (n < 0 || n >= NSIG || *end) {
            return -1;
        }
        *num = n;
        return 0;
    }

    if (strncmp(str, "RTMIN+", 6) == 0) {
        char* end;
        long n = strtol(str + 6, &end, 10);
        if (n < 1 || n >= SIGRTMAX - SIGRTMIN || *end) {
            return -1;
        }
        *num = SIGRTMIN + n;
        return 0;
    } else if (strncmp(str, "RTMAX-", 6) == 0) {
        char* end;
        long n = strtol(str + 6, &end, 10);
        if (n < 1 || n >= SIGRTMAX - SIGRTMIN || *end) {
            return -1;
        }
        *num = SIGRTMAX - n;
        return 0;
    }

    for (int i = 0; i < NSIG; i++) {
        if (__signalnames[i] && strcmp(str, __signalnames[i]) == 0) {
            *num = i;
            return 0;
        }
    }

    return -1;
}
