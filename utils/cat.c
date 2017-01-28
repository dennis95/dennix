/* Copyright (c) 2017 Dennis Wölfing
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

/* utils/cat.c
 * Concatenates files.
 */

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static bool failed = false;

static void cat(const char* path) {
    int fd;
    if (strcmp(path, "-") == 0) {
        fd = 0;
    } else {
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            warn("'%s'", path);
            failed = true;
            return;
        }
    }

    while (true) {
        char buffer[1];
        ssize_t readSize = read(fd, buffer, sizeof(buffer));
        if (readSize < 0) {
            warn("'%s'", path);
            failed = true;
            break;
        } else if (readSize == 0) {
            break;
        }

        ssize_t writtenSize = write(1, buffer, readSize);
        if (writtenSize < 0) {
            err(1, "write");
        }
    }

    if (fd != 0) {
        close(fd);
    }
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            cat(argv[i]);
        }
    } else {
        cat("-");
    }

    return failed ? 1 : 0;
}
