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

/* libc/src/unistd/_exit.c
 * Exits the application without cleanup.
 */

#include <unistd.h>
#include <dennix/exit.h>
#include <sys/syscall.h>

DEFINE_SYSCALL(SYSCALL_EXIT_THREAD, __noreturn void, exit_thread,
        (const struct exit_thread*));

__noreturn void _exit(int status) {
    struct exit_thread data = {0};
    data.flags = EXIT_PROCESS;
    data.status = status;
    exit_thread(&data);
}
