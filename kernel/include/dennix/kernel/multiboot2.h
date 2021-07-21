/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/include/dennix/kernel/multiboot2.h
 * Definition of the multiboot2 structure.
 */

#ifndef KERNEL_MULTIBOOT2_H
#define KERNEL_MULTIBOOT2_H

#include <dennix/kernel/kernel.h>

struct multiboot_info {
    uint32_t total_size;
    uint32_t reserved;
};

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_module {
    multiboot_tag header;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[];
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
};

struct multiboot_tag_mmap {
    multiboot_tag header;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot_mmap_entry entries[];
};

struct multiboot_tag_framebuffer {
    multiboot_tag header;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
    // Additional fields omitted.
};

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MODULE 3
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

#define MULTIBOOT_MEMORY_AVAILABLE 1

#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

#endif
