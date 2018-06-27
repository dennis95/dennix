/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

/* libc/include/sys/stat.h
 * File information.
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/cdefs.h>
#define __need_blkcnt_t
#define __need_blksize_t
#define __need_dev_t
#define __need_gid_t
#define __need_ino_t
#define __need_mode_t
#define __need_nlink_t
#define __need_off_t
#define __need_time_t
#define __need_uid_t
#include <sys/libc-types.h>
#include <dennix/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 070
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXO 07
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01
#define S_ISUID 04000
#define S_ISGID 02000

#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec

int fstat(int, struct stat*);
int fstatat(int, const char* __restrict, struct stat* __restrict, int);
int lstat(const char* __restrict, struct stat* __restrict);
int mkdir(const char*, mode_t);
int mkdirat(int, const char*, mode_t);
int stat(const char* __restrict, struct stat* __restrict);

#ifdef __cplusplus
}
#endif

#endif
