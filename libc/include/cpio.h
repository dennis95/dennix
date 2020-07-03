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

/* libc/include/cpio.h
 * cpio archive format.
 */

#ifndef _CPIO_H
#define _CPIO_H

#include <sys/cdefs.h>

#define C_IRUSR 0400
#define C_IWUSR 0200
#define C_IXUSR 0100
#define C_IRGRP 040
#define C_IWGRP 020
#define C_IXGRP 010
#define C_IROTH 04
#define C_IWOTH 02
#define C_IXOTH 01
#define C_ISUID 04000
#define C_ISGID 02000
#define C_ISVTX 01000

#define C_ISFIFO 010000
#define C_ISCHR 020000
#define C_ISDIR 040000
#define C_ISBLK 060000
#define C_ISREG 0100000
#define C_ISCTG 0110000
#define C_ISLNK 0120000
#define C_ISSOCK 0140000

#define MAGIC "070707"

#endif
