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

/* libc/include/wchar.h
 * Wide characters.
 */

#ifndef _WCHAR_H
#define _WCHAR_H

#include <sys/cdefs.h>
#define __need___va_list
#include <stdarg.h>
#define __need_NULL
#define __need_size_t
#define __need_wchar_t
#define __need_wint_t
#if __USE_DENNIX || __USE_POSIX
#  define __need_FILE
#endif
#include <sys/libc-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if __USE_POSIX || __USE_DENNIX
/* POSIX requires us to define va_list but <stdarg.h> does not allow us to
   request only va_list. */
#  ifndef _VA_LIST_DEFINED
typedef __gnuc_va_list va_list;
#    define _VA_LIST_DEFINED
#  endif
#endif

typedef struct {
    unsigned int _state;
    wchar_t _wc;
} mbstate_t;

size_t mbrtowc(wchar_t* __restrict, const char* __restrict, size_t,
        mbstate_t* __restrict);
size_t mbsrtowcs(wchar_t* __restrict, const char** __restrict, size_t,
        mbstate_t* __restrict);

#ifdef __cplusplus
}
#endif

#endif