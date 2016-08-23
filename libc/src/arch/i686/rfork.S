/* Copyright (c) 2016, Dennis Wölfing
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

/* libc/src/arch/i686/rfork.S
 * Creates a new process.
 */

.global rfork
.type rfork, @function
rfork:
    push %ebp
    mov %esp, %ebp
    sub $12, %esp

    # Push struct regfork
    push %ebp
    push %edi
    push %esi
    push %edx
    push %ecx
    push %ebx
    # In the child process this returns 0
    push $0
    push $afterFork
    # The value for %esp does not matter because leave will modify %esp anyway
    push $0

    push %esp
    push 8(%ebp)

    call regfork
afterFork:
    leave
    ret
.size rfork, . - rfork
