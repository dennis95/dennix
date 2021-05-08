/* Copyright (c) 2016, 2017, 2019, 2020, 2021 Dennis Wölfing
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

/* kernel/include/dennix/termios.h
 * Terminal I/O.
 */

#ifndef _DENNIX_TERMIOS_H
#define _DENNIX_TERMIOS_H

/* Not all of the following modes are actually implemented. Many of them only
   make sense for terminals connected over a serial line and have no useful
   meaning for software terminals. */

/* Input modes */
#define BRKINT (1 << 0) /* unimplemented */
#define ICRNL (1 << 1) /* unimplemented */
#define IGNBRK (1 << 2) /* unimplemented */
#define IGNCR (1 << 3) /* unimplemented */
#define IGNPAR (1 << 4) /* unimplemented */
#define INLCR (1 << 5) /* unimplemented */
#define INPCK (1 << 6) /* unimplemented */
#define ISTRIP (1 << 7) /* unimplemented */
#define IXANY (1 << 8) /* unimplemented */
#define IXOFF (1 << 9) /* unimplemented */
#define IXON (1 << 10) /* unimplemented */
#define PARMRK (1 << 11) /* unimplemented */

/* Output modes */
#define OPOST (1 << 0) /* unimplemented */

/* Control modes */
#define CLOCAL (1 << 0) /* unimplemented */
#define CREAD (1 << 1)
#define CS5 (0 << 2) /* unimplemented */
#define CS6 (1 << 2) /* unimplemented */
#define CS7 (2 << 2) /* unimplemented */
#define CS8 (3 << 2)
#define CSIZE (CS5 | CS6 | CS7 | CS8)
#define CSTOPB (1 << 4) /* unimplemented */
#define HUPCL (1 << 5) /* unimplemented */
#define PARENB (1 << 6) /* unimplemented */
#define PARODD (1 << 7) /* unimplemented */

/* Local modes */
#define ECHO (1 << 0)
#define ICANON (1 << 1)
#define ISIG (1 << 2)
#define ECHONL (1 << 3)
/* Non-standard flag that causes the terminal to transmit key-codepoint pairs
   instead of bytes. See struct kbwc in <dennix/kbkeys.h>. Disables most
   terminal-specific processing of input. */
#define _KBWC (1 << 4)
#define ECHOE (1 << 5)
#define ECHOK (1 << 6)
#define IEXTEN (1 << 7) /* unimplemented */
#define NOFLSH (1 << 8)
#define TOSTOP (1 << 9) /* unimplemented */

/* Baud rates */
#define B0 0 /* unimplemented */
#define B50 50 /* unimplemented */
#define B75 75 /* unimplemented */
#define B110 110 /* unimplemented */
#define B134 134 /* unimplemented */
#define B150 150 /* unimplemented */
#define B200 200 /* unimplemented */
#define B300 300 /* unimplemented */
#define B600 600 /* unimplemented */
#define B1200 1200 /* unimplemented */
#define B1800 1800 /* unimplemented */
#define B2400 2400 /* unimplemented */
#define B4800 4800 /* unimplemented */
#define B9600 9600 /* unimplemented */
#define B19200 19200 /* unimplemented */
#define B38400 38400 /* unimplemented */

#define VEOF 0
#define VEOL 1
#define VERASE 2
#define VINTR 3
#define VKILL 4
#define VMIN 5
#define VQUIT 6
#define VSTART 7 /* unimplemented */
#define VSTOP 8 /* unimplemented */
#define VSUSP 9 /* unimplemented */
#define VTIME 10 /* unimplemented */
#define NCCS 11

#define TCSAFLUSH 0
#define TCSANOW 1
#define TCSADRAIN 2

#define TCIFLUSH 0
#define TCIOFLUSH 1
#define TCOFLUSH 2

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    speed_t c_ispeed;
    speed_t c_ospeed;
    cc_t c_cc[NCCS];
};

#endif
