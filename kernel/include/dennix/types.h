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

/* kernel/include/dennix/types.h
 * Data types.
 */

#ifndef _DENNIX_TYPES_H
#define _DENNIX_TYPES_H

typedef __INTMAX_TYPE__ __blkcnt_t;
typedef long __blksize_t;
typedef unsigned int __clockid_t;
typedef unsigned long __dev_t;
typedef __UINT64_TYPE__ __gid_t;
typedef __UINT64_TYPE__ __id_t;
typedef __UINTMAX_TYPE__ __ino_t;
typedef int __mode_t;
typedef unsigned int __nlink_t;
typedef __INTMAX_TYPE__ __off_t;
typedef int __pid_t;
typedef __SIZE_TYPE__ __reclen_t;
typedef __INT64_TYPE__ __time_t;
typedef __UINT64_TYPE__ __uid_t;

#endif
