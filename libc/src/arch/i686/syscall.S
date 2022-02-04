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

/* libc/src/arch/i686/syscall.S
 * Syscall caller.
 */

.section .text
.global __syscall
.type __syscall, @function
__syscall:
    push %ebp
    mov %esp, %ebp

    # Preserve caller saved registers
    push %edi
    push %esi
    push %ebx
    sub $12, %esp

    # Put the arguments into registers
    mov 8(%ebp), %ebx
    mov 12(%ebp), %ecx
    mov 16(%ebp), %edx
    mov 20(%ebp), %esi
    mov 24(%ebp), %edi

    int $0x30

    # Set errno if it was changed. The kernel sets %ecx to 0 if errno is
    # unchanged and puts errno into %ecx otherwise.
    test %ecx, %ecx
    jz 1f
    mov %ecx, %gs:errno@ntpoff

1:  add $12, %esp
    pop %ebx
    pop %esi
    pop %edi
    pop %ebp
    ret
.size __syscall, . - __syscall
