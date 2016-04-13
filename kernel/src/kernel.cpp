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

/* kernel/src/kernel.cpp
 * The kernel's main function.
 */

#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/process.h>

static void processA() {
    while (true) Log::printf("A");
}

static void processB() {
    while (true) Log::printf("B");
}

extern "C" void kmain(uint32_t /*magic*/, paddr_t multibootAddress) {
    Log::printf("Hello World!\n");
    AddressSpace::initialize();
    Log::printf("Address space initialized!\n");

    multiboot_info* multiboot = (multiboot_info*) kernelSpace->map(
            multibootAddress, PAGE_PRESENT | PAGE_WRITABLE);

    PhysicalMemory::initialize(multiboot);
    Log::printf("Physical Memory initialized\n");

    kernelSpace->unmap((vaddr_t) multiboot);

    Process::initialize();
    Process::startProcess((void*) processA);
    Process::startProcess((void*) processB);
    Log::printf("Processes initialized\n");

    Interrupts::initPic();
    Interrupts::enable();
    Log::printf("Interrupts enabled!\n");

    while (true) {
        asm volatile ("hlt");
    }
}
