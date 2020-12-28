/* Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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
#include <dennix/kernel/display.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/multiboot.h>
#include <dennix/kernel/terminal.h>
#include <dennix/kernel/terminaldisplay.h>

static size_t callback(void*, const char* s, size_t nBytes);

// To avoid early memory allocations we statically allocate enough space for
// the display.
char displayBuf[sizeof(Display)];

void Log::earlyInitialize(multiboot_info* multiboot) {
    if (!(multiboot->flags & MULTIBOOT_FRAMEBUFFER_INFO) ||
            multiboot->framebuffer_type == MULTIBOOT_EGA_TEXT) {
        vaddr_t videoMapped = kernelSpace->mapPhysical(0xB8000, PAGESIZE,
                PROT_READ | PROT_WRITE);
        video_mode mode;
        mode.video_bpp = 0;
        mode.video_height = 25;
        mode.video_width = 80;
        TerminalDisplay::display = new (displayBuf) Display(mode,
                (char*) videoMapped, 160);
    } else if (multiboot->framebuffer_type == MULTIBOOT_RGB &&
            (multiboot->framebuffer_bpp == 24 ||
            multiboot->framebuffer_bpp == 32)) {
        vaddr_t lfbMapping;
        size_t mapSize;
        vaddr_t lfb = kernelSpace->mapUnaligned(multiboot->framebuffer_addr,
                multiboot->framebuffer_height * multiboot->framebuffer_pitch,
                PROT_READ | PROT_WRITE, lfbMapping, mapSize);
        if (!lfb) {
            // This shouldn't fail in practise as enough memory should be
            // available.
            asm ("hlt");
        }

        video_mode mode;
        mode.video_bpp = multiboot->framebuffer_bpp;
        mode.video_height = multiboot->framebuffer_height;
        mode.video_width = multiboot->framebuffer_width;
        TerminalDisplay::display = new (displayBuf) Display(mode, (char*) lfb,
                multiboot->framebuffer_pitch);
    } else {
        // Without any usable display we cannot do anything.
        while (true) asm volatile ("cli; hlt");
    }
}

void Log::initialize() {
    TerminalDisplay::display->initialize();
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
    return terminal->write(s, nBytes, 0);
}
