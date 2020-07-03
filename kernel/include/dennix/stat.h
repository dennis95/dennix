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

/* kernel/include/dennix/stat.h
 * File information.
 */

#ifndef _DENNIX_STAT_H
#define _DENNIX_STAT_H

#include <dennix/timespec.h>
#include <dennix/types.h>

/* We define these macros to their traditional values because some software
   assumes these values. */
#define S_IFIFO 010000
#define S_IFCHR 020000
#define S_IFDIR 040000
#define S_IFBLK 060000
#define S_IFREG 0100000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000
/* The following values are unused and can be used for new file types:
   030000 050000 070000 080000 090000 0130000 0150000 0160000 0170000
   The value 0110000 is reserved for contiguous files. */

#define S_IFMT 0170000

#define UTIME_NOW (-1)
#define UTIME_OMIT (-2)

struct stat {
    __dev_t st_dev;
    __ino_t st_ino;
    __mode_t st_mode;
    __nlink_t st_nlink;
    __uid_t st_uid;
    __gid_t st_gid;
    __dev_t st_rdev;
    __off_t st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    __blksize_t st_blksize;
    __blkcnt_t st_blocks;
};

#endif
