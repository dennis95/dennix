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

/* kernel/src/arch/x86_64/syscall.S
 * Syscall handler.
 */

.section .text
.global syscallHandler
.type syscallHandler, @function
syscallHandler:
    cld

    mov $0x10, %r10w
    mov %r10w, %ds
    mov %r10w, %es

    sub $8, %rsp

    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %r8
    push %r9

    mov %rax, %rdi
    call getSyscallHandler

    pop %r9
    pop %r8
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi

    movl $0, errno
    call *%rax

    add $8, %rsp

    mov errno, %edi

    # Check whether signals are pending.
    mov signalPending, %r10
    test %r10, %r10
    jnz 2f

    mov $0x23, %r10w
    mov %r10w, %ds
    mov %r10w, %es

1:  iretq

# Fake an InterruptContext so that we can call handleSignal.
2:  sub $16, %rsp
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    mov %rsp, %rdi
    call handleSignal
    mov %rax, %rsp

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    add $16, %rsp

    jmp 1b
.size syscallHandler, . - syscallHandler
