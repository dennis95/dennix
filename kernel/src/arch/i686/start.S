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

/* kernel/src/arch/i686/start.S
 * Contains the multiboot header and the entry point.
 */

#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_FLAGS 0x3
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.section multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

.section .text
.global _start
.type _start, @function
_start:
    # Load the GDT.
    mov $gdt_descriptor, %ecx
    lgdt (%ecx)
    mov $0x10, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs
    mov %cx, %ss
    ljmp $0x8, $1f

1:  mov $kernel_stack, %esp

    push $0
    push $0
    mov %esp, %ebp

    push %ebx # multiboot
    push %eax # magic

    call _init

    call kmain

_halt:
    cli
    hlt
    jmp _halt
.size _start, . - _start

.section .bss
.align 4096
.skip 4096
kernel_stack:
