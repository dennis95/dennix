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

/* libc/src/sys/mman/mmap.c
 * Map pages.
 */

#include <sys/mman.h>
#include <sys/syscall.h>

DEFINE_SYSCALL(SYSCALL_MMAP, void*, sys_mmap, (struct __mmapRequest*));

void* mmap(void* addr, size_t size, int protection, int flags, int fd,
        off_t offset) {
    struct __mmapRequest request = {
        ._addr = addr,
        ._size = size,
        ._protection = protection,
        ._flags = flags,
        ._fd = fd,
        ._offset = offset,
    };

    return sys_mmap(&request);
}
