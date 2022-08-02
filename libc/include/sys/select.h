/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/include/sys/select.h
 * I/O multiplexing.
 */

#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#include <sys/cdefs.h>
#define __need_time_t
#define __need_suseconds_t
#include <bits/types.h>
#include <bits/timeval.h>
#include <dennix/sigset.h>
#include <dennix/timespec.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FD_SETSIZE 1024

typedef struct {
    unsigned int __bits[FD_SETSIZE / (8 * sizeof(unsigned int))];
} fd_set;

#define __FD_INDEX(fd) ((fd) / (8 * sizeof(unsigned int)))
#define __FD_BIT(fd) (1U << ((fd) % (8 * sizeof(unsigned int))))

#define FD_CLR(fd, fdset) \
        ((void) ((fdset)->__bits[__FD_INDEX(fd)] &= ~__FD_BIT(fd)))
#define FD_ISSET(fd, fdset) ((fdset)->__bits[__FD_INDEX(fd)] & __FD_BIT(fd))
#define FD_SET(fd, fdset) \
        ((void) ((fdset)->__bits[__FD_INDEX(fd)] |= __FD_BIT(fd)))
#define FD_ZERO(fdset) ((void) __builtin_memset(fdset, 0, sizeof(*(fdset))))

int pselect(int, fd_set* __restrict, fd_set* __restrict, fd_set* __restrict,
        const struct timespec* __restrict, const sigset_t* __restrict);
int select(int, fd_set* __restrict, fd_set* __restrict, fd_set* __restrict,
        struct timeval* __restrict);

#ifdef __cplusplus
}
#endif

#endif
