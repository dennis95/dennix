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

/* libc/include/sys/syscall.h
 * Syscall definition macros.
 */

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <dennix/syscall.h>

#define _SYSCALL_TOSTRING(x) #x

#define _SYSCALL_BODY(number, name) \
    asm("\n" \
    ".pushsection .text\n" \
    #name ":\n\t" \
        "mov $" _SYSCALL_TOSTRING(number) ", %eax\n\t" \
        "jmp __syscall\n" \
    ".popsection\n")

#define DEFINE_SYSCALL(number, type, name, params) \
    _SYSCALL_BODY(number, name); \
    extern type name params

#define DEFINE_SYSCALL_GLOBAL(number, type, name, params) \
    asm(".global " #name); \
    DEFINE_SYSCALL(number, type, name, params)

#define DEFINE_SYSCALL_WEAK_ALIAS(target, alias) \
    asm(".weak " #alias "\n" \
    ".set " #alias ", " #target); \
    extern __typeof(target) alias

#endif
