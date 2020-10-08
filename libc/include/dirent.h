/* Copyright (c) 2016, 2017, 2018, 2020 Dennis Wölfing
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
#define __need_ino_t
#define __need_reclen_t
#define __need_size_t
#define __need_ssize_t
#include <bits/types.h>
#if __USE_DENNIX
#  include <dennix/dent.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Our code assumes that this struct matches exactly struct posix_dent. */
struct dirent {
    ino_t d_ino;
    reclen_t d_reclen;
    unsigned char d_type;
    __extension__ char d_name[];
};

#define _DIRENT_HAVE_D_RECLEN
#define _DIRENT_HAVE_D_TYPE

typedef struct __DIR DIR;

#ifdef __is_dennix_libc
struct __DIR {
    int fd;
    size_t bufferFilled;
    size_t offsetInBuffer;
    char buffer[32768];
};
#endif

int alphasort(const struct dirent**, const struct dirent**);
int closedir(DIR*);
DIR* fdopendir(int);
DIR* opendir(const char*);
struct dirent* readdir(DIR*);
void rewinddir(DIR*);
int scandir(const char*, struct dirent***, int (*)(const struct dirent*),
        int (*)(const struct dirent**, const struct dirent**));

#if __USE_DENNIX
#  define IFTODT(mode) _IFTODT(mode)
#  define DTTOIF(type) _DTTOIF(type)

ssize_t posix_getdents(int, void*, size_t, int);
#endif

#ifdef __cplusplus
}
#endif

#endif
