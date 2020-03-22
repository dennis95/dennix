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

/* libc/include/pwd.h
 * User list.
 */

#ifndef _PWD_H
#define _PWD_H

#include <sys/cdefs.h>
#define __need_gid_t
#define __need_size_t
#define __need_uid_t
#include <bits/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct passwd {
    char* pw_name;
    uid_t pw_uid;
    gid_t pw_gid;
    char* pw_dir;
    char* pw_shell;
};

struct passwd* getpwnam(const char*);

#ifdef __cplusplus
}
#endif

#endif
