/* Copyright (c) 2019 Dennis Wölfing
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

/* libc/src/arch/x86_64/setjmp.S
 * Nonlocal jumps.
 */

.global setjmp
.type setjmp, @function
setjmp:
    mov %rbx, 0(%rdi)
    mov %rbp, 8(%rdi)
    mov %rsp, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %r13, 32(%rdi)
    mov %r14, 40(%rdi)
    mov %r15, 48(%rdi)
    mov (%rsp), %rax
    mov %rax, 56(%rdi)
    mov $0, %rax
    ret
.size setjmp, . - setjmp

.global longjmp
.type longjmp, @function
longjmp:
    mov 0(%rdi), %rbx
    mov 8(%rdi), %rbp
    mov 16(%rdi), %rsp
    mov 24(%rdi), %r12
    mov 32(%rdi), %r13
    mov 40(%rdi), %r14
    mov 48(%rdi), %r15
    mov 56(%rdi), %rax
    mov %rax, (%rsp)

    mov %rsi, %rax
    test %rax, %rax
    jnz 1f
    inc %rax
1:  ret
.size longjmp, . - longjmp
