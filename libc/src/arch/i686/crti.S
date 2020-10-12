/* Copyright (c) 2016, 2020 Dennis Wölfing
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

/* libc/src/arch/i686/crti.S
 * Provides _init and _fini symbols to call global ctors/dtors.
 */

# We are currently transitioning to init_array/fini_array. For now we support
# both the old and the new method of calling global constructors/destructors.
# Once we fully switch to init_array we can rewrite this code in C.

.section .init
.global _init
_init:
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %esi

    mov $__init_array_start, %ebx
    mov $__init_array_end, %esi
1:  cmp %ebx, %esi
    je 2f
    call *(%ebx)
    add $4, %ebx
    jmp 1b

2:  # GCC will call global constructors here

.section .fini
.global _fini
_fini:
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %esi

    mov $__fini_array_start, %ebx
    mov $__fini_array_end, %esi
1:  cmp %ebx, %esi
    je 2f
    call *(%ebx)
    add $4, %ebx
    jmp 1b

2:  # GCC will call global destructors here
