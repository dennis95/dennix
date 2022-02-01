/* Copyright (c) 2016, 2022 Dennis Wölfing
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

/* libc/src/errno/initProgname.c
 * Initializes the program name.
 */

#include <errno.h>
#include <stddef.h>

char* __program_invocation_name;
char* __program_invocation_short_name;
__weak_alias(__program_invocation_name, program_invocation_name);
__weak_alias(__program_invocation_short_name, program_invocation_short_name);

void __initProgname(char* argv[]) {
    __program_invocation_name = argv[0] ? argv[0] : "";
    __program_invocation_short_name = __program_invocation_name;

    // Get the last part of argv[0].
    char* s = __program_invocation_name;
    while (*s) {
        if (*s++ == '/') {
            __program_invocation_short_name = s;
        }
    }
}
