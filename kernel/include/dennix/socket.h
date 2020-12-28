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

/* kernel/include/dennix/socket.h
 * Sockets.
 */

#ifndef _DENNIX_SOCKET_H
#define _DENNIX_SOCKET_H

#include <dennix/types.h>

struct sockaddr {
    __sa_family_t sa_family;
    __extension__ char sa_data[];
};

struct sockaddr_storage {
    __sa_family_t ss_family;
    char __data[104];
};

#define AF_UNSPEC 0
#define AF_UNIX 1

#define SOCK_CLOEXEC (1 << 0)
#define SOCK_CLOFORK (1 << 1)
#define SOCK_NONBLOCK (1 << 2)

#define _SOCK_FLAGS (SOCK_CLOEXEC | SOCK_CLOFORK | SOCK_NONBLOCK)

#define SOCK_STREAM (1 << 3)

#endif
