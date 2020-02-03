/* Copyright (c) 2020 Dennis Wölfing
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

/* kernel/src/arch/x86-family/earlypanic.S
 * Early panic.
 */

#include <dennix/kernel/arch.h>

.code32
.section bootstrap_text, "ax"
.global earlyPanic
.type earlyPanic, @function
earlyPanic:
    # Tries to print an error message to the screen and halts. The error message
    # is expected in eax and a pointer to the multiboot structure in esi.
    # We cannot run any C code because initialization was not completed.
    testl $0x1000, (%esi)
    jz text
    cmpb $2, 109(%esi)
    je text
    cmpb $1, 109(%esi)
    je graphics
    # We cannot print anything when the bootloader chooses some weird mode.
    jmp halt

text:
    mov $0xB8000, %ebx
1:  mov (%eax), %cl
    test %cl, %cl
    jz halt
    mov %cl, (%ebx)
    movb $0x4F, 1(%ebx) # white on red
    inc %eax
    add $2, %ebx
    jmp 1b

graphics:
    # We only support 24 and 32 bpp.
    cmpb $24, 108(%esi)
    je 1f
    cmpb $32, 108(%esi)
    jne halt

1:  xor %ebx, %ebx
    mov 108(%esi), %bl
    mov %ebx, %esp # bpp
    shr $3, %ebx # bpp / 8

    # Get the LFB address.
    mov 88(%esi), %edi
    # Get the physical address of the vgafont.
    mov $(vgafont - KERNEL_VIRTUAL), %ebp

1:  xor %ecx, %ecx
    mov (%eax), %cl
    test %cl, %cl
    jz halt
    shl $4, %cx
    add %ebp, %ecx
    # ecx now contains the address of the correct glyph in the vgafont.

2:  mov $0x10000, %edx
    movb (%ecx), %dl
3:  test $0x80, %dl
    jz 4f
    # White foreground
    movb $0xFF, 0(%edi)
    movb $0xFF, 1(%edi)
    movb $0xFF, 2(%edi)
    jmp 5f
    # Red background
4:  movb $0x00, 0(%edi)
    movb $0x00, 1(%edi)
    movb $0xAA, 2(%edi)
5:  add %ebx, %edi
    shl %edx
    test $0xFF0000, %edx
    jnz 3b

    # Fill the one pixel wide space between characters.
    movb $0x00, 0(%edi)
    movb $0x00, 1(%edi)
    movb $0xAA, 2(%edi)

    sub %esp, %edi
    add 96(%esi), %edi
    inc %ecx
    mov %ebp, %edx
    xor %ecx, %edx
    test $0xF, %edx
    jnz 2b

    mov 96(%esi), %edx
    shl $4, %edx
    sub %edx, %edi
    add %esp, %edi
    add %ebx, %edi

    inc %eax
    jmp 1b
halt:
    hlt
.size earlyPanic, . - earlyPanic
