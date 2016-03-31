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
#include <dennix/kernel/kernel.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
} PACKED;

#define GDT_ENTRY(base, limit, access, flags) { \
    (limit) & 0xFFFF, \
    (base) & 0xFFFF, \
    ((base) >> 16) & 0xFF, \
    (access) & 0xFF, \
    (((limit) >> 16) & 0x0F) | ((flags) & 0xF0), \
    ((base) >> 24) & 0xFF \
}

#define GDT_PRESENT ((1 << 7) | (1 << 4))
#define GDT_RING0 0
#define GDT_EXECUTABLE (1 << 3)
#define GDT_READ_WRITE (1 << 1)

#define GDT_GRANULARITY_4K (1 << 7)
#define GDT_PROTECTED_MODE (1 << 6)

gdt_entry gdt[] = {
    // Null Segment
    GDT_ENTRY(0, 0, 0, 0),

    // Kernel Code Segment
    GDT_ENTRY(0, 0xFFFFFFF,
            GDT_PRESENT | GDT_RING0 | GDT_EXECUTABLE | GDT_READ_WRITE,
            GDT_GRANULARITY_4K | GDT_PROTECTED_MODE),

    // Kernel Data Segment
    GDT_ENTRY(0, 0xFFFFFFF,
            GDT_PRESENT | GDT_RING0 | GDT_READ_WRITE,
            GDT_GRANULARITY_4K | GDT_PROTECTED_MODE),
};

extern "C" {
struct {
    uint16_t size;
    void* gdtp;
} PACKED gdt_descriptor = {
    sizeof(gdt) - 1,
    gdt
};
}
