/* Copyright (c) 2020, 2021 Dennis Wölfing
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

/* kernel/include/dennix/dent.h
 * Directory entries.
 */

#ifndef _DENNIX_DENT_H
#define _DENNIX_DENT_H

#include <dennix/types.h>

struct posix_dent {
    __ino_t d_ino;
    __reclen_t d_reclen;
    unsigned char d_type;
    __extension__ char d_name[];
};

#define _IFTODT(mode) (((mode) & 0170000) >> 12)
#define _DTTOIF(type) (((type) & 017) << 12)

#define DT_FIFO 01
#define DT_CHR 02
#define DT_DIR 04
#define DT_BLK 06
#define DT_REG 010
#define DT_LNK 012
#define DT_SOCK 014
#define DT_UNKNOWN 0

#define DT_MQ 020
#define DT_SEM 020
#define DT_SHM 020

#define DT_FORCE_TYPE (1 << 0)

#define _DT_FLAGS DT_FORCE_TYPE

#endif
