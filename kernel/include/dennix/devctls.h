/* Copyright (c) 2018, 2019, 2020, 2021, 2022 Dennis Wölfing
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
   are defined here. More devctl numbers are defined in headers listed below. */

#define TIOCSCTTY _DEVCTL(_IOCTL_VOID, 0)
/* _IOCTL_VOID 1 and 2 are used in <dennix/display.h>. */

/* _IOCTL_INT 0 is used in <dennix/display.h>. */
#define TCFLSH _DEVCTL(_IOCTL_INT, 1)
/* _IOCTL_INT 2 is used in <dennix/mouse.h>. */

#define TIOCGWINSZ _DEVCTL(_IOCTL_PTR, 0) /* (struct winsize*) */
#define TIOCGPGRP _DEVCTL(_IOCTL_PTR, 1) /* (pid_t*) */
#define TIOCSPGRP _DEVCTL(_IOCTL_PTR, 2) /* (const pid_t*) */
/* _IOCTL_PTR 3 - 6 are used in <dennix/display.h>. */
#define TIOCGPATH _DEVCTL(_IOCTL_PTR, 7) /* (char*) */
#define TIOCSWINSZ _DEVCTL(_IOCTL_PTR, 8) /* (const struct winsize*) */

#endif
