/* Copyright (c) 2017 Dennis Wölfing
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

/* libc/src/sys/utsname/uname.c
 * System name.
 */

#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

int uname(struct utsname* result) {
    confstr(_CS_UNAME_SYSNAME, result->sysname, sizeof(result->sysname));
    strcpy(result->nodename, "dennix");
    confstr(_CS_UNAME_RELEASE, result->release, sizeof(result->release));
    confstr(_CS_UNAME_VERSION, result->version, sizeof(result->version));
    confstr(_CS_UNAME_MACHINE, result->machine, sizeof(result->machine));
    return 0;
}
