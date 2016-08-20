/* Copyright (c) 2016, Dennis Wölfing
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

/* kernel/include/dennix/mman.h
 * Memory management.
 */

#ifndef _DENNIX_MMAN_H
#define _DENNIX_MMAN_H

#define PROT_READ (1 << 0)
#define PROT_WRITE (1 << 1)
#define PROT_EXEC (1 << 2)
#define PROT_NONE 0

#define _PROT_FLAGS (PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE)

#define MAP_PRIVATE (1 << 0)
#define MAP_ANONYMOUS (1 << 1)

#define MAP_FAILED ((void*) 0)

#if defined(__is_dennix_kernel) || defined(__is_dennix_libc)
/* The mmap() function has to many parameters to be passed in registers */
#  include <stddef.h>
#  include <dennix/types.h>
struct __mmapRequest {
    void* _addr;
    size_t _size;
    int _protection;
    int _flags;
    int _fd;
    __off_t _offset;
};
#endif

#endif
