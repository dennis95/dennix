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

/* kernel/src/arch/x86_64/interrupts.S
 * Interrupt handlers.
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

.type commonHandler, @function
commonHandler:
    cld

    # Push registers
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

    # Switch to kernel data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es

    mov %rsp, %rdi
    call handleInterrupt
    mov %rax, %rsp

    # Check whether signals are pending.
    mov signalPending, %rbx
    test %rbx, %rbx
    jz 1f

    # Don't handle signals when returning to kernelspace.
    mov 144(%rsp), %rbx # cs
    cmp $0x8, %rbx
    je 1f

    mov %rsp, %rdi
    call handleSignal
    mov %rax, %rsp

    # Switch back to user data segment
1:  mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es

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

    # Remove error code and interrupt number from stack
    add $16, %rsp

    iretq
.size commonHandler, . - commonHandler

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
    # This code is mapped in all user address spaces. When a userspace
    # program returns from a signal handler it will return to this address and
    # and then perform a sigreturn.
    int $0x32
.global endSigreturn
endSigreturn:
