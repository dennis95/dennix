/* Copyright (c) 2019, 2020 Dennis Wölfing
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

/* kernel/src/arch/x86_64/start.S
 * Kernel entry point.
 */

#define CR0_FPU_EMULATION (1 << 2)
#define CR0_FPU_EXCEPTIONS (1 << 5)
#define CR0_WRITE_PROTECT (1 << 16)
#define CR0_PAGING_ENABLE (1 << 31)
#define CR4_PAE_ENABLE (1 << 5)
#define CR4_SSE_ENABLE (1 << 9)
#define CR4_SSE_EXCEPTIONS (1 << 10)

#define EFLAGS_ID (1 << 21)
#define CPUID_EXTENDED_FEATURES 0x80000001
#define CPUID_EXT_EDX_LONG_MODE (1 << 29)

#define MSR_EFER 0xC0000080
#define EFER_LONG_MODE_ENABLE (1 << 8)
#define EFER_NO_EXECUTE (1 << 11)

#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_FLAGS 0x7
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

#define PAGE_READONLY 0x1
#define PAGE_WRITE 0x3
#define PAGE_HIGH_NO_EXECUTE (1 << 31)

#define GRAPHICS 0
#define TEXT 1

# Choose preferred video settings. The bootloader is still allowed to choose a
# different video mode.
#define VIDEO_MODE GRAPHICS
#define PREFERRED_WIDTH 1024
#define PREFERRED_HEIGHT 768

.section multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM
.skip 20
.long VIDEO_MODE
.long PREFERRED_WIDTH
.long PREFERRED_HEIGHT
.long 32 # bpp

.code32
.section bootstrap_text, "ax"
.global _start
.type _start, @function
_start:
    cld

    # Save the multiboot magic and pointer for later.
    mov %eax, %ebp
    mov %ebx, %esi

    # Check whether we are running on an x86_64 CPU.
    # We are temporarily abusing kernelPml4 as a stack because pushf needs one.
    mov $(kernelPml4 + 4), %esp
    pushf
    mov (%esp), %eax
    xorl $EFLAGS_ID, (%esp)
    popf
    pushf
    cmp (%esp), %eax
    je unsupportedCpu

    mov $CPUID_EXTENDED_FEATURES, %eax
    cpuid
    test $CPUID_EXT_EDX_LONG_MODE, %edx
    jz unsupportedCpu

    # Initialize paging structures.
    movl $(bootstrapPdpt + PAGE_WRITE), kernelPml4
    movl $(bootstrapPageDir + PAGE_WRITE), bootstrapPdpt
    movl $(bootstrapPageTable + PAGE_WRITE), bootstrapPageDir

    movl $(kernelBasePdpt + PAGE_WRITE), kernelPml4 + 0x800
    movl $(kernelBasePageDir + PAGE_WRITE), kernelBasePdpt
    movl $(kernelBasePageTable + PAGE_WRITE), kernelBasePageDir

    # Recursive mapping
    movl $(kernelPml4 + PAGE_WRITE), kernelPml4 + 0xFF0

    movl $(kernelPdpt + PAGE_WRITE), kernelPml4 + 0xFF8
    movl $(kernelPageDir + PAGE_WRITE), kernelPdpt + 0xFF0
    movl $(kernelPageTable + PAGE_WRITE), kernelPageDir

    # Identity-map the bootstrap section
    mov $numBootstrapPages, %ecx
    mov $(bootstrapPageTable + 256 * 8), %edi
    mov $(bootstrapBegin + PAGE_WRITE), %edx

1:  mov %edx, (%edi)
    add $8, %edi
    add $0x1000, %edx
    loop 1b

    # Map the executable part of the kernel
    mov $numExecPages, %ecx
    add $(kernelPageTable - bootstrapPageTable), %edi
    mov $(kernelPhysicalBegin + PAGE_READONLY), %edx

1:  mov %edx, (%edi)
    add $8, %edi
    add $0x1000, %edx
    loop 1b

    # Map the readonly part of the kernel
    mov $numReadOnlyPages, %ecx
    sub $numExecPages, %ecx

1:  mov %edx, (%edi)
    movl $PAGE_HIGH_NO_EXECUTE, 4(%edi)
    add $8, %edi
    add $0x1000, %edx
    loop 1b

    # Map the writable part of the kernel
    mov $numWritablePages, %ecx
    or $PAGE_WRITE, %edx

1:  mov %edx, (%edi)
    movl $PAGE_HIGH_NO_EXECUTE, 4(%edi)
    add $8, %edi
    add $0x1000, %edx
    loop 1b

    # Map video memory at 0xFFFFFFFF80000000.
    movl $0xB8003, kernelPageTable
    movl $PAGE_HIGH_NO_EXECUTE, kernelPageTable + 4

    # Enable paging and long mode.
    mov %cr4, %ecx
    or $CR4_PAE_ENABLE, %ecx
    mov %ecx, %cr4

    mov $kernelPml4, %ecx
    mov %ecx, %cr3

    mov $MSR_EFER, %ecx
    rdmsr
    or $(EFER_LONG_MODE_ENABLE | EFER_NO_EXECUTE), %eax
    wrmsr

    mov %cr0, %ecx
    or $(CR0_WRITE_PROTECT | CR0_PAGING_ENABLE), %ecx
    mov %ecx, %cr0

    lgdt fakeGdtDescriptor
    ljmp $0x8, $1f

.code64
    # Jump into the higher half.
1:  jmp _start2
.size _start, . - _start

.code32
unsupportedCpu:
    mov $errorMsg, %eax
    jmp earlyPanic

.section bootstrap_rodata, "a"
errorMsg:
    .asciz "This version of Dennix requires an x86_64 CPU."

fakeGdt:
    .skip 8
    .word 0xFFFF
    .word 0
    .byte 0
    .byte 0x9A
    .byte 0xAF
    .byte 0

fakeGdtDescriptor:
    .word 15
    .long fakeGdt

.section bootstrap_bss, "aw", @nobits
.align 4096
bootstrapPdpt:
    .skip 4096
bootstrapPageDir:
    .skip 4096
bootstrapPageTable:
    .skip 4096
# Anything below is not freed after initialization.
.global kernelPml4
kernelPml4:
    .skip 4096
kernelBasePdpt:
    .skip 4096
kernelBasePageDir:
    .skip 4096
kernelBasePageTable:
    .skip 4096
kernelPdpt:
    .skip 4096
kernelPageDir:
    .skip 4096
kernelPageTable:
    .skip 4096

.code64
.section .text
.type _start2, @function
_start2:
    mov %ebp, %r12d # magic
    mov %esi, %r13d # multiboot

    mov $kernel_stack, %rsp

    # Put the TSS address into the GDT.
    mov $tss, %rcx
    mov %cx, gdt + 40 + 2
    shr $16, %rcx
    mov %cl, gdt + 40 + 4
    mov %ch, gdt + 40 + 7
    shr $16, %rcx
    mov %ecx, gdt + 40 + 8

    # Load the GDT.
    push $gdt
    pushw gdt_size

    lgdt (%rsp)
    mov $0x10, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs
    mov %cx, %ss
    mov $0x2B, %cx
    ltr %cx

    # Load the IDT
    push $idt
    pushw idt_size

    lidt (%rsp)

    # Initialize the x87 FPU.
    mov %cr0, %rcx
    and $(~CR0_FPU_EMULATION), %rcx
    or $CR0_FPU_EXCEPTIONS, %rcx
    mov %rcx, %cr0
    fninit

    # Initialize SSE.
    mov %cr4, %rcx
    or $(CR4_SSE_ENABLE | CR4_SSE_EXCEPTIONS), %rcx
    mov %rcx, %cr4
    push $0x1F80
    ldmxcsr (%rsp)
    fxsave initFpu

    mov $kernel_stack, %rsp

    push $0
    push $0
    mov %rsp, %rbp

    call _init

    mov %r12, %rdi # magic
    mov %r13, %rsi # multiboot

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
