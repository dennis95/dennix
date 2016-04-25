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

/* kernel/src/gdt.cpp
 * Defines the GDT.
 */

#include <stdint.h>
#include <dennix/kernel/process.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
};

struct tss_entry {
    uint32_t prev;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint16_t reserved;
    uint16_t iomapBase;
};

#define GDT_ENTRY(base, limit, access, flags) { \
    (limit) & 0xFFFF, \
    (base) & 0xFFFF, \
    ((base) >> 16) & 0xFF, \
    (access) & 0xFF, \
    (((limit) >> 16) & 0x0F) | ((flags) & 0xF0), \
    ((base) >> 24) & 0xFF \
}

#define GDT_ACCESSED (1 << 0)
#define GDT_READ_WRITE (1 << 1)
#define GDT_EXECUTABLE (1 << 3)
#define GDT_SEGMENT (1 << 4)
#define GDT_RING0 (0 << 5)
#define GDT_RING3 (3 << 5)
#define GDT_PRESENT (1 << 7)

#define GDT_GRANULARITY_4K (1 << 7)
#define GDT_PROTECTED_MODE (1 << 6)

extern "C" {

tss_entry tss = {
    /*.prev =*/ 0,
    /*.esp0 =*/ 0,
    /*.ss0 =*/ 0x10,
    /*.esp1 =*/ 0,
    /*.ss1 =*/ 0,
    /*.esp2 =*/ 0,
    /*.ss2 =*/ 0,
    /*.cr3 =*/ 0,
    /*.eip =*/ 0,
    /*.eflags =*/ 0,
    /*.eax =*/ 0,
    /*.ecx =*/ 0,
    /*.edx =*/ 0,
    /*.ebx =*/ 0,
    /*.esp =*/ 0,
    /*.ebp =*/ 0,
    /*.esi =*/ 0,
    /*.edi =*/ 0,
    /*.es; =*/ 0,
    /*.cs; =*/ 0,
    /*.ss; =*/ 0,
    /*.ds; =*/ 0,
    /*.fs; =*/ 0,
    /*.gs; =*/ 0,
    /*.ldtr =*/ 0,
    /*.reserved =*/ 0,
    /*.iomapBase =*/ 0,
};

gdt_entry gdt[] = {
    // Null Segment
    GDT_ENTRY(0, 0, 0, 0),

    // Kernel Code Segment
    GDT_ENTRY(0, 0xFFFFFFF,
            GDT_PRESENT | GDT_SEGMENT | GDT_RING0 | GDT_EXECUTABLE |
            GDT_READ_WRITE,
            GDT_GRANULARITY_4K | GDT_PROTECTED_MODE),

    // Kernel Data Segment
    GDT_ENTRY(0, 0xFFFFFFF,
            GDT_PRESENT | GDT_SEGMENT | GDT_RING0 | GDT_READ_WRITE,
            GDT_GRANULARITY_4K | GDT_PROTECTED_MODE),

    // User Code Segment
    GDT_ENTRY(0, 0xFFFFFFF,
            GDT_PRESENT | GDT_SEGMENT | GDT_RING3 | GDT_EXECUTABLE |
            GDT_READ_WRITE,
            GDT_GRANULARITY_4K | GDT_PROTECTED_MODE),

    // User Data Segment
    GDT_ENTRY(0, 0xFFFFFFF,
            GDT_PRESENT | GDT_SEGMENT | GDT_RING3 | GDT_READ_WRITE,
            GDT_GRANULARITY_4K | GDT_PROTECTED_MODE),

    // Task State Segment
    GDT_ENTRY(/*(uintptr_t) &tss*/ 0, sizeof(tss) - 1,
            GDT_PRESENT | /*GDT_RING3 |*/ GDT_EXECUTABLE | GDT_ACCESSED, 0),
};

uint16_t gdt_size = sizeof(gdt) - 1;
}

void setKernelStack(uintptr_t stack) {
    tss.esp0 = stack;
}
