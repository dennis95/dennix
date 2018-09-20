/* Copyright (c) 2018 Dennis Wölfing
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

/* libc/include/endian.h
 * System endianness.
 */

#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <sys/cdefs.h>
#include <stdint.h>

#define BYTE_ORDER __BYTE_ORDER__
#define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define PDP_ENDIAN __ORDER_PDP_ENDIAN__

#define __bswap16(x) __builtin_bswap16(x)
#define __bswap32(x) __builtin_bswap32(x)
#define __bswap64(x) __builtin_bswap64(x)

#if BYTE_ORDER == LITTLE_ENDIAN
#  define htobe16(x) __bswap16(x)
#  define htole16(x) (x)
#  define be16toh(x) __bswap16(x)
#  define le16toh(x) (x)
#  define htobe32(x) __bswap32(x)
#  define htole32(x) (x)
#  define be32toh(x) __bswap32(x)
#  define le32toh(x) (x)
#  define htobe64(x) __bswap64(x)
#  define htole64(x) (x)
#  define be64toh(x) __bswap64(x)
#  define le64toh(x) (x)
#elif BYTE_ORDER == BIG_ENDIAN
#  define htobe16(x) (x)
#  define htole16(x) __bswap16(x)
#  define be16toh(x) (x)
#  define le16toh(x) __bswap16(x)
#  define htobe32(x) (x)
#  define htole32(x) __bswap32(x)
#  define be32toh(x) (x)
#  define le32toh(x) __bswap32(x)
#  define htobe64(x) (x)
#  define htole64(x) __bswap64(x)
#  define be64toh(x) (x)
#  define le64toh(x) __bswap64(x)
#else
#  error "Unsupported endianness."
#endif

#endif
