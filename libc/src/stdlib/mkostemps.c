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

/* libc/src/stdlib/mkostemps.c
 * Create a temporary file.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t attempts = 0;
static const char* letters =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int mkostemps(char* template, int suffixLength, int flags) {
    size_t length = strlen(template);
    if (length < (size_t) suffixLength + 6) {
        errno = EINVAL;
        return -1;
    }

    char* xtemplate = template + length - suffixLength - 6;
    if (strncmp(xtemplate, "XXXXXX", 6) != 0) {
        errno = EINVAL;
        return -1;
    }

    do {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        // Produce a value that is unlikely to be the same for multiple
        // invocations. Only the least significant 36 bits matter.
        uint64_t value = ts.tv_sec ^ ((uint64_t) ts.tv_nsec << 6) ^
                ((uint64_t) getpid() << 16) ^ attempts++;
        for (size_t i = 0; i < 6; i++) {
            xtemplate[i] = letters[value % 64];
            value /= 64;
        }

        // TODO: Because O_EXCL is not actually implemented, there is no
        // guarantee that the file did not exist before.
        int fd = open(template, O_RDWR | O_CREAT | O_EXCL | flags, 0600);
        if (fd >= 0) return fd;
    } while (errno == EEXIST);

    return -1;
}
