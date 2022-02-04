/* Copyright (c) 2019, 2022 Dennis Wölfing
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

/* libc/src/arch/x86_64/crt0.S
 * Program initialization.
 */

.section .text
.global _start
.type _start, @function
_start:
    # argc in rdi, argv in rsi, envp in rdx
    push $0
    push $0
    mov %rsp, %rbp

    push %rdi
    push %rsi
    push %rdx
    sub $8, %rsp

    mov %rdx, __environ

    call _init

    mov 16(%rsp), %rdi
    call __initProgname
    call __initializeThreads

    add $8, %rsp
    pop %rdx
    pop %rsi
    pop %rdi

    call main

    mov %rax, %rdi
    call exit
.size _start, . - _start
