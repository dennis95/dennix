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

/* libc/include/fcntl.h
 * File control.
 */

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/cdefs.h>
#define __need_mode_t
#define __need_off_t
#define __need_pid_t
#include <sys/libc-types.h>
#include <sys/stat.h>
#include <dennix/fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

int open(const char*, int, ...);
int openat(int, const char*, int, ...);

#ifdef __cplusplus
}
#endif

#endif
