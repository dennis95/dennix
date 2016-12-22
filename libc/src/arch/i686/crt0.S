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

/* libc/src/arch/i686/crt0.S
 * Program initialization.
 */

.section .text
.global _start
.type _start, @function
_start:
    # The kernel has put argc into eax, argv into ebx and envp into ecx.

    # Create a stack frame
    push $0
    push $0
    mov %esp, %ebp

    sub $12, %esp
    push %ecx # envp
    push %ebx # argv
    push %eax # argc

    # Push argv for __initProgname
    sub $12, %esp
    push %ebx # argv

    # Set environ
    mov %ecx, environ

    # Call global constructors
    call _init

    # Initialize program_invocation_name
    call __initProgname
    add $16, %esp

    call main

    add $4, %esp
    push %eax
    call exit


.size _start, . - _start
