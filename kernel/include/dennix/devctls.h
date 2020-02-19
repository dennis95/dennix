/* Copyright (c) 2018, 2019, 2020 Dennis Wölfing
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

/* kernel/include/dennix/devctls.h
 * Device Control.
 */

#ifndef _DENNIX_DEVCTLS_H
#define _DENNIX_DEVCTLS_H

#include <dennix/devctl.h>
#include <dennix/winsize.h>

/* Devctl numbers that are defined by default in <devctl.h> and <sys/ioctl.h>
   are defined here. More devctl numbers are defined in <dennix/display.h>. */

#define TIOCGWINSZ _DEVCTL(_IOCTL_PTR, 0) /* (struct winsize*) */
#define TIOCGPGRP _DEVCTL(_IOCTL_PTR, 1) /* (pid_t*) */
#define TIOCSPGRP _DEVCTL(_IOCTL_PTR, 2) /* (const pid_t*) */

#endif
