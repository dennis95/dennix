/* Copyright (c) 2016, 2018 Dennis Wölfing
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

/* libc/include/ctype.h
 * Character types.
 */

#ifndef _CTYPE_H
#define _CTYPE_H

#include <sys/cdefs.h>
#define __need_locale_t
#include <sys/libc-types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern __gnu_inline int isblank(int _c) {
    return _c == '\t' || _c == ' ';
}

extern __gnu_inline int iscntrl(int _c) {
    return (_c >= '\0' && _c <= '\x1F') || _c == '\x7F';
}

extern __gnu_inline int isdigit(int _c) {
    return _c >= '0' && _c <= '9';
}

extern __gnu_inline int islower(int _c) {
    return _c >= 'a' && _c <= 'z';
}

extern __gnu_inline int ispunct(int _c) {
    return (_c >= '!' && _c <= '/') || (_c >= ':' && _c <= '@') ||
            (_c >= '[' && _c <= '`') || (_c >= '{' && _c <= '~');
}

extern __gnu_inline int isspace(int _c) {
    return _c == '\t' || _c == '\n' || _c == '\v' || _c == '\f' ||
            _c == '\r' || _c == ' ';
}

extern __gnu_inline int isupper(int _c) {
    return _c >= 'A' && _c <= 'Z';
}

extern __gnu_inline int isalpha(int _c) {
    return isupper(_c) || islower(_c);
}

extern __gnu_inline int isalnum(int _c) {
    return isalpha(_c) || isdigit(_c);
}

extern __gnu_inline int isgraph(int _c) {
    return isalnum(_c) || ispunct(_c);
}

extern __gnu_inline int isprint(int _c) {
    return isgraph(_c) || _c == ' ';
}

extern __gnu_inline int isxdigit(int _c) {
    return isdigit(_c) || (_c >= 'A' && _c <= 'F') || (_c >= 'a' && _c <= 'f');
}

extern __gnu_inline int tolower(int _c) {
    if (isupper(_c)) return _c + 0x20;
    return _c;
}

extern __gnu_inline int toupper(int _c) {
    if (islower(_c)) return _c - 0x20;
    return _c;
}

#ifdef __cplusplus
}
#endif

#endif
