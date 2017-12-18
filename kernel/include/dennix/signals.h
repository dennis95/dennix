/* Copyright (c) 2017 Dennis Wölfing
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

/* kernel/include/dennix/signals.h
 * Signal numbers.
 */

#ifndef _DENNIX_SIGNALS_H
#define _DENNIX_SIGNALS_H

/* These signal numbers are commonly used by applications. POSIX specifies
   these numbers in the kill(1) utility as part of the XSI option. */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGABRT 6
#define SIGKILL 9
#define SIGALRM 14
#define SIGTERM 15

#define SIGBUS 4
#define SIGCHLD 5
#define SIGCONT 7
#define SIGFPE 8
#define SIGILL 10
#define SIGPIPE 11
#define SIGSEGV 12
#define SIGSTOP 13
#define SIGSYS 16
#define SIGTRAP 17
#define SIGTSTP 18
#define SIGTTIN 19
#define SIGTTOU 20
#define SIGURG 21
#define SIGUSR1 22
#define SIGUSR2 23

#define _NSIG 24

#endif
