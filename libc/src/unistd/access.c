/* Copyright (c) 2016, 2019, 2022 Dennis Wölfing
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

/* libc/src/unistd/access.c
 * Checks accessibility of a file. (POSIX2008, called from C89)
 */

#define stat __stat
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

int __access(const char* path, int mode) {
    struct stat st;
    if (stat(path, &st) < 0) {
        return -1;
    }

    bool accessible = true;

    if (mode & R_OK) {
        accessible &= !!(st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH));
    }
    if (mode & W_OK) {
        accessible &= !!(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH));
    }
    if (mode & X_OK) {
        accessible &= !!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
    }

    if (!accessible) {
        errno = EACCES;
        return -1;
    }
    return 0;
}
__weak_alias(__access, access);
