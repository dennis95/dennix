/* Copyright (c) 2020 Dennis Wölfing
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

/* kernel/include/dennix/poll.h
 * Polling files.
 */

#ifndef _DENNIX_POLL_H
#define _DENNIX_POLL_H

struct pollfd {
    int fd;
    short events;
    short revents;
};

typedef unsigned int nfds_t;

#define POLLIN (1 << 0)
#define POLLRDNORM (1 << 1)
#define POLLRDBAND (1 << 2)
#define POLLPRI (1 << 3)
#define POLLOUT (1 << 4)
#define POLLWRNORM (1 << 5)
#define POLLWRBAND (1 << 6)
#define POLLERR (1 << 7)
#define POLLHUP (1 << 8)
#define POLLNVAL (1 << 9)

#endif