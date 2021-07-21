/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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

/* kernel/src/log.h
 * Defines functions to print to the screen.
 */

#include <stdio.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/console.h>
#include <dennix/kernel/display.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/multiboot2.h>

static size_t callback(void*, const char* s, size_t nBytes);

// To avoid early memory allocations we statically allocate enough space for
// the display.
char displayBuf[sizeof(Display)];

void Log::earlyInitialize(const multiboot_info* multiboot) {
    uintptr_t p = (uintptr_t) multiboot + 8;
    const multiboot_tag* tag;

    while (true) {
        tag = (const multiboot_tag*) p;
        if (tag->type == MULTIBOOT_TAG_TYPE_END ||
                tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            break;
        }

        p = ALIGNUP(p + tag->size, 8);
    }

    const multiboot_tag_framebuffer* fbTag =
            (const multiboot_tag_framebuffer*) tag;

    if (tag->type == MULTIBOOT_TAG_TYPE_END ||
            fbTag->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
        vaddr_t videoMapped = kernelSpace->mapPhysical(0xB8000, PAGESIZE,
                PROT_READ | PROT_WRITE);
        video_mode mode;
        mode.video_bpp = 0;
        mode.video_height = 25;
        mode.video_width = 80;
        console->display = new (displayBuf) Display(mode,
                (char*) videoMapped, 160);
        console->updateDisplaySize();
    } else if (fbTag->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB &&
            (fbTag->framebuffer_bpp == 24 || fbTag->framebuffer_bpp == 32)) {
        vaddr_t lfbMapping;
        size_t mapSize;
        vaddr_t lfb = kernelSpace->mapUnaligned(fbTag->framebuffer_addr,
                fbTag->framebuffer_height * fbTag->framebuffer_pitch,
                PROT_READ | PROT_WRITE, lfbMapping, mapSize);
        if (!lfb) {
            // This shouldn't fail in practise as enough memory should be
            // available.
            asm ("hlt");
        }

        video_mode mode;
        mode.video_bpp = fbTag->framebuffer_bpp;
        mode.video_height = fbTag->framebuffer_height;
        mode.video_width = fbTag->framebuffer_width;
        console->display = new (displayBuf) Display(mode, (char*) lfb,
                fbTag->framebuffer_pitch);
        console->updateDisplaySize();
    } else {
        // Without any usable display we cannot do anything.
        while (true) asm volatile ("cli; hlt");
    }
}

void Log::initialize() {
    console->display->initialize();
}

void Log::printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    Log::vprintf(format, ap);
    va_end(ap);
}

void Log::vprintf(const char* format, va_list ap) {
    vcbprintf(nullptr, callback, format, ap);
}

static size_t callback(void*, const char* s, size_t nBytes) {
    return console->write(s, nBytes, 0);
}
