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

/* kernel/src/arch/i686/interrupts.S
 * Contains stubs that are called when an interrupt happens.
 */

.section .text

.macro isr no
.global isr_\no
isr_\no:
    push $0 # no error code
    push $\no
    jmp commonHandler
.endm

.macro isr_error_code no
.global isr_\no
isr_\no:
    # an error code was already pushed
    push $\no
    jmp commonHandler
.endm

commonHandler:
    cld

    # Push registers
    push %ebp
    push %edi
    push %esi
    push %edx
    push %ecx
    push %ebx
    push %eax

    # Switch to kernel data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es

    mov %esp, %eax
    and $(~0xFF), %esp # Align the stack
    sub $12, %esp
    push %eax

    call handleInterrupt
    mov %eax, %esp

    # Check whether signals are pending.
    mov signalPending, %ebx
    test %ebx, %ebx
    jz 1f

    # Don't handle signals when returning to kernelspace.
    mov 40(%esp), %ebx # cs
    cmp $0x8, %ebx
    je 1f

    and $(~0xFF), %esp # Align the stack
    sub $12, %esp
    push %eax
    call handleSignal
    mov %eax, %esp

    # Switch back to user data segment
1:  mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es

    pop %eax
    pop %ebx
    pop %ecx
    pop %edx
    pop %esi
    pop %edi
    pop %ebp

    # Remove error code and interrupt number from stack
    add $8, %esp

    iret

# CPU Exceptions
isr 0             # Devide-by-zero Error
isr 1             # Debug
isr 2             # Non-maskable Interrupt
isr 3             # Breakpoint
isr 4             # Overflow
isr 5             # Bound Range Exceeded
isr 6             # Invalid Opcode
isr 7             # Device Not Available
isr_error_code 8  # Double Fault
isr 9             # Coprocessor Segment Overrun
isr_error_code 10 # Invalid TSS
isr_error_code 11 # Segment Not Present
isr_error_code 12 # Stack-Segement Fault
isr_error_code 13 # General Protection Fault
isr_error_code 14 # Page Fault
#isr 15           # Reserved
isr 16            # x87 Floating-Point Exception
isr_error_code 17 # Alignment Check
isr 18            # Machine Check
isr 19            # SIMD Floating-Point Exception
isr 20            # Virtualization Exception

# IRQs
isr 32
isr 33
isr 34
isr 35
isr 36
isr 37
isr 38
isr 39
isr 40
isr 41
isr 42
isr 43
isr 44
isr 45
isr 46
isr 47

#isr 48 # Syscall
isr 49 # Schedule
isr 50 # sigreturn

.global beginSigreturn
beginSigreturn:
    # This section is mapped in all user address spaces. When a userspace
    # program returns from a signal handler it will return to this address and
    # and then perform a sigreturn.
    int $0x32
.global endSigreturn
endSigreturn:
