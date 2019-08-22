/* Copyright (c) 2016, 2017, 2018, 2019 Dennis Wölfing
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

/* kernel/include/dennix/kernel/kernel.h
 * Common definitions for the kernel.
 */

#ifndef KERNEL_KERNEL_H
#define KERNEL_KERNEL_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define ALIGNED(alignment) __attribute__((__aligned__(alignment)))
#define NORETURN __attribute__((__noreturn__))
#define PACKED __attribute__((__packed__))
#define restrict __restrict
#define ALWAYS_INLINE inline __attribute__((__always_inline__))

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ALIGNUP(val, alignment) ((((val) - 1) & ~((alignment) - 1)) + (alignment))

// Define an incomplete type for symbols so we can only take their addresses
typedef struct _incomplete_type symbol_t;

typedef uintptr_t paddr_t;
typedef uintptr_t vaddr_t;

// Placement new
inline void* operator new(size_t /*size*/, void* addr) {
    return addr;
}
inline void* operator new[](size_t /*size*/, void* addr) {
    return addr;
}
inline void operator delete(void*, void*) {};
inline void operator delete[](void*, void*) {};

#endif
