/* Copyright (c) 2016, 2021 Dennis Wölfing
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

/* libc/include/errno.h
 * Error numbers.
 */

#ifndef _ERRNO_H
#define _ERRNO_H

#include <sys/cdefs.h>
#include <dennix/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__is_dennix_libk) || defined(__is_dennix_kernel)
extern int* __errno_location;
#  define errno (*__errno_location)
#else
extern int errno;
#  define errno errno
#endif

#if __USE_DENNIX
extern char* program_invocation_name;
extern char* program_invocation_short_name;
#endif

#ifdef __cplusplus
}
#endif

#endif
