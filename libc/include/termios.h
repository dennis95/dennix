/* Copyright (c) 2016, 2018, 2019, 2021, 2024 Dennis Wölfing
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

/* libc/include/termios.h
 * Terminal I/O.
 */

#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/cdefs.h>
#define __need_pid_t
#include <bits/types.h>
#include <dennix/termios.h>
#if __USE_DENNIX || __USE_POSIX >= 202405L
#  include <dennix/winsize.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

speed_t cfgetispeed(const struct termios*);
speed_t cfgetospeed(const struct termios*);
int cfsetispeed(struct termios*, speed_t);
int cfsetospeed(struct termios*, speed_t);
int tcflush(int, int);
int tcgetattr(int, struct termios*);
int tcsetattr(int, int, const struct termios*);

#if __USE_DENNIX || _USE_POSIX >= 202405L
int tcgetwinsize(int, struct winsize*);
int tcsetwinsize(int, const struct winsize*);
#endif

#if __USE_DENNIX
int tcsetsid(int, pid_t);
#endif

#ifdef __cplusplus
}
#endif

#endif
