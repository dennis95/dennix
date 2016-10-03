/* Copyright (c) 2016, Dennis Wölfing
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

/* libc/src/dirent/readdir.c
 * Reads directory contents.
 */

#include <dirent.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>

DEFINE_SYSCALL(SYSCALL_READDIR, ssize_t, sys_readdir,
        (int, unsigned long, void*, size_t));

struct dirent* readdir(DIR* dir) {
    if (!dir->dirent) {
        dir->dirent = malloc(sizeof(struct dirent));
        dir->dirent->d_reclen = sizeof(struct dirent);
    }

    size_t entrySize = dir->dirent->d_reclen;
    ssize_t size = sys_readdir(dir->fd, dir->offset, dir->dirent, entrySize);

    if (size < 0) {
        return NULL;
    } else if (size == 0) {
        return NULL;
    } else if ((size_t) size <= entrySize) {
        dir->offset++;
        return dir->dirent;
    } else {
        free(dir->dirent);
        dir->dirent = malloc((size_t) size);
        sys_readdir(dir->fd, dir->offset, dir->dirent, (size_t) size);
        dir->offset++;
        return dir->dirent;
    }
}
