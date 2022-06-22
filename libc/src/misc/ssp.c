/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/src/misc/ssp.c
 * Stack Smashing Protector.
 */

#define arc4random_buf __arc4random_buf
#define open __open
#define write __write
#include <fcntl.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>

uintptr_t __stack_chk_guard = 0x00123456;

#ifndef __is_dennix_libk
__attribute__((constructor, optimize("no-stack-protector")))
static void initSsp(void) {
    uintptr_t guard;
    arc4random_buf(&guard, sizeof(guard));
    __stack_chk_guard = guard;
}

static const char errorMessage[] = "*** stack smashing detected ***\n";

noreturn void __stack_chk_fail(void) {
    int fd = open("/dev/tty", O_WRONLY | O_CLOEXEC | O_CLOFORK | O_NONBLOCK);
    if (fd >= 0) {
        write(fd, errorMessage, sizeof(errorMessage) - 1);
    }
    abort();
}
__weak_alias(__stack_chk_fail, __stack_chk_fail_local);
#endif
