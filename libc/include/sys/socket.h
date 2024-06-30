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

/* libc/include/sys/socket.h
 * Sockets.
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <sys/cdefs.h>
#define __need_sa_family_t
#define __need_size_t
#define __need_socklen_t
#define __need_ssize_t
#include <bits/types.h>
#include <dennix/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

int accept(int, struct sockaddr* __restrict, socklen_t* __restrict);
int bind(int, const struct sockaddr*, socklen_t);
int connect(int, const struct sockaddr*, socklen_t);
int listen(int, int);
int socket(int, int, int);

#if __USE_DENNIX || __USE_POSIX >= 202405L
int accept4(int, struct sockaddr* __restrict, socklen_t* __restrict, int);
#endif

#ifdef __cplusplus
}
#endif

#endif
