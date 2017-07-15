/* Copyright (c) 2016, 2017 Dennis Wölfing
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

#define CR0_WRITE_PROTECT (1 << 16)
#define CR0_PAGING_ENABLE (1 << 31)

#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_FLAGS 0x3
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

#define PAGE_READONLY 0x1
#define PAGE_WRITE 0x3

.section multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

.section bootstrap_text, "ax"
.global _start
.type _start, @function
_start:
    cld

    # Add the page tables to the page directory
    movl $(pageTableBootstrap + PAGE_WRITE), kernelPageDirectory
    movl $(pageTableKernel + PAGE_WRITE), kernelPageDirectory + 0xC00
    movl $(pageTablePhysicalMemory + PAGE_WRITE), kernelPageDirectory + 0xFF8

    # Recursively map the page directory at 0xFFC00000
    movl $(kernelPageDirectory + PAGE_WRITE), kernelPageDirectory + 0xFFC

    # Identity-map the bootstrap section
    mov $numBootstrapPages, %ecx
    mov $(pageTableBootstrap + 256 * 4), %edi
    mov $(bootstrapBegin + PAGE_WRITE), %edx

1:  mov %edx, (%edi)
    add $4, %edi
    add $0x1000, %edx
    loop 1b

    # Map readonly part of the kernel
    mov $numReadOnlyPages, %ecx
    add $(pageTableKernel - pageTableBootstrap), %edi
    mov $(kernelPhysicalBegin + PAGE_READONLY), %edx

1:  mov %edx, (%edi)
    add $4, %edi
    add $0x1000, %edx
    loop 1b

    # Map writable part
    mov $numWritablePages, %ecx
    or $PAGE_WRITE, %edx

1:  mov %edx, (%edi)
    add $4, %edi
    add $0x1000, %edx
    loop 1b

    # Map video memory at 0xC0000000.
    movl $0xB8003, pageTableKernel

    # Enable paging
    mov $kernelPageDirectory, %ecx
    mov %ecx, %cr3

    mov %cr0, %ecx
    or $(CR0_WRITE_PROTECT | CR0_PAGING_ENABLE), %ecx
    mov %ecx, %cr0

    # Jump into the higher half
    jmp _start2
.size _start, . - _start

.section bootstrap_bss, "aw", @nobits
.align 4096
.global kernelPageDirectory
kernelPageDirectory:
    .skip 4096
pageTableBootstrap:
    .skip 4096
pageTableKernel:
    .skip 4096
pageTablePhysicalMemory:
    .skip 4096 # The physical memory manager needs this page table.

.section .text
.type _start2, @function
_start2:
    mov $kernel_stack, %esp

    # Put the TSS address into the GDT.
    mov $tss, %ecx
    mov %cx, gdt + 40 + 2
    shr $16, %ecx
    mov %cl, gdt + 40 + 4
    mov %ch, gdt + 40 + 7

    # Load the GDT.
    push $gdt
    pushw gdt_size

    lgdt (%esp)
    mov $0x10, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs
    mov %cx, %ss
    ljmp $0x8, $1f

1:  mov $0x2B, %cx
    ltr %cx

    # Load the IDT
    push $idt
    pushw idt_size

    lidt (%esp)

    mov $kernel_stack, %esp

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
.size _start2, . - _start2

.section .bss
.align 4096
.skip 4096
kernel_stack:
