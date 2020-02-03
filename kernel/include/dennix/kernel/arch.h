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

/* kernel/include/dennix/kernel/arch.h
 * Architecture specific definitions. This file must use preprocessor constructs
 * only so that it can be included from non-C files.
 */

#ifndef KERNEL_ARCH_H
#define KERNEL_ARCH_H

#ifdef __i386__
#  define KERNEL_PHYSICAL 0x100000
#  define KERNEL_VIRTUAL 0xC0000000
#elif defined(__x86_64__)
#  define KERNEL_PHYSICAL 0x100000
#  define KERNEL_VIRTUAL 0xFFFFFFFF80000000
#endif

#endif
