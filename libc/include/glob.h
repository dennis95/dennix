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

/* libc/include/glob.h
 * Pathname pattern matching.
 */

#ifndef _GLOB_H
#define _GLOB_H

#include <sys/cdefs.h>
#define __need_size_t
#include <bits/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t gl_pathc;
    char** gl_pathv;
    size_t gl_offs;
} glob_t;

#define GLOB_APPEND (1 << 0)
#define GLOB_DOOFFS (1 << 1)
#define GLOB_ERR (1 << 2)
#define GLOB_MARK (1 << 3)
#define GLOB_NOCHECK (1 << 4)
#define GLOB_NOESCAPE (1 << 5)
#define GLOB_NOSORT (1 << 6)

#define GLOB_ABORTED 1
#define GLOB_NOMATCH 2
#define GLOB_NOSPACE 3

int glob(const char* __restrict, int, int(*)(const char*, int),
        glob_t* __restrict);
void globfree(glob_t*);

#ifdef __cplusplus
}
#endif

#endif
