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

/* libc/src/pwd/getpwnam.c
 * Get user information by name.
 */

#include <pwd.h>
#include <string.h>

// We currently do not support multiple users, so we just hardcode this entry.
static const struct passwd userEntry = {
    .pw_name = "user",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_dir = "/home/user",
    .pw_shell = "/bin/sh"
};

struct passwd* getpwnam(const char* name) {
    if (strcmp(name, "user") == 0) {
        return (struct passwd*) &userEntry;
    }
    return NULL;
}
