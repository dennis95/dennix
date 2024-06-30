/* Copyright (c) 2020, 2024 Dennis Wölfing
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

/* libc/include/poll.h
 * Polling files.
 */

#ifndef _POLL_H
#define _POLL_H

#include <sys/cdefs.h>
#include <dennix/poll.h>
#if __USE_DENNIX || __USE_POSIX >= 202405L
#  include <dennix/sigset.h>
#  include <dennix/timespec.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int poll(struct pollfd[], nfds_t, int);

#if __USE_DENNIX || __USE_POSIX >= 202405L
int ppoll(struct pollfd[], nfds_t, const struct timespec* __restrict,
        const sigset_t* __restrict);
#endif

#ifdef __cplusplus
}
#endif

#endif
