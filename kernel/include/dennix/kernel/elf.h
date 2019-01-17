/* Copyright (c) 2016, 2019 Dennis Wölfing
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

/* kernel/include/dennix/kernel/elf.h
 * ELF format.
 */

#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include <stdint.h>

typedef uintptr_t Elf_Addr;
#ifdef __i386__
typedef uint32_t Elf_Off;
#elif defined(__x86_64__)
typedef uint64_t Elf_Off;
#endif

struct ElfHeader {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    Elf_Addr e_entry;
    Elf_Off e_phoff;
    Elf_Off e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct ProgramHeader32 {
    uint32_t p_type;
    Elf_Off p_offset;
    Elf_Addr p_vaddr;
    Elf_Addr p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct ProgramHeader64 {
    uint32_t p_type;
    uint32_t p_flags;
    Elf_Off p_offset;
    Elf_Addr p_vaddr;
    Elf_Addr p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

#ifdef __i386__
typedef ProgramHeader32 ProgramHeader;
#elif defined(__x86_64__)
typedef ProgramHeader64 ProgramHeader;
#endif

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

#endif
