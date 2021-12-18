/* Copyright (c) 2021 Dennis Wölfing
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

/* libc/include/fnmatch.h
 * Pattern matching.
 */

#ifndef _FNMATCH_H
#define _FNMATCH_H

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FNM_NOMATCH 1
#define FNM_PATHNAME (1 << 0)
#define FNM_PERIOD (1 << 1)
#define FNM_NOESCAPE (1 << 2)
#define FNM_CASEFOLD (1 << 3)
#define FNM_IGNORECASE FNM_CASEFOLD

int fnmatch(const char*, const char*, int);

#ifdef __cplusplus
}
#endif

#endif
