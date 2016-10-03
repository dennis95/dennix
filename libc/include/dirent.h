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

/* libc/include/dirent.h
 * Directory definitions.
 */

#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/cdefs.h>
#include <dennix/dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __DIR DIR;

#ifdef __is_dennix_libc
struct __DIR {
    int fd;
    struct dirent* dirent;
    unsigned long offset;
};
#endif

int closedir(DIR*);
DIR* fdopendir(int);
DIR* opendir(const char*);
struct dirent* readdir(DIR*);

#ifdef __cplusplus
}
#endif

#endif
