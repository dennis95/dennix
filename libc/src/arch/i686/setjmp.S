/* Copyright (c) 2018 Dennis Wölfing
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

/* libc/src/arch/i686/setjmp.S
 * Nonlocal jumps.
 */

.global setjmp
.type setjmp, @function
setjmp:
    mov 4(%esp), %ecx
    mov %ebx, 0(%ecx)
    mov %esi, 4(%ecx)
    mov %edi, 8(%ecx)
    mov %ebp, 12(%ecx)
    mov %esp, 16(%ecx)
    mov (%esp), %eax
    mov %eax, 20(%ecx)
    mov $0, %eax
    ret
.size setjmp, . - setjmp

.global longjmp
.type longjmp, @function
longjmp:
    mov 8(%esp), %eax
    mov 4(%esp), %ecx
    mov 0(%ecx), %ebx
    mov 4(%ecx), %esi
    mov 8(%ecx), %edi
    mov 12(%ecx), %ebp
    mov 16(%ecx), %esp
    mov 20(%ecx), %ecx
    mov %ecx, (%esp)

    test %eax, %eax
    jnz 1f
    inc %eax
1:  ret
.size longjmp, . - longjmp
