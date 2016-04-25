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

#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/process.h>

static void processA() {
    asm volatile ("int $0x30" :: "a"(0), "b"(0));
    __builtin_unreachable();
}

static void processB() {
    asm volatile ("int $0x30" :: "a"(0), "b"(42));
    __builtin_unreachable();
}

static Process* startProcesses(void* function) {
    AddressSpace* addressSpace = kernelSpace->fork();
    paddr_t phys = PhysicalMemory::popPageFrame();
    void* processCode = (void*) addressSpace->map(phys, PAGE_PRESENT | PAGE_USER);
    vaddr_t processMapped = kernelSpace->map(phys, PAGE_PRESENT | PAGE_WRITABLE);
    memcpy((void*) processMapped, function, 0x1000);
    kernelSpace->unmap(processMapped);
    return Process::startProcess(processCode, addressSpace);
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
    startProcesses((void*) processA);
    startProcesses((void*) processB);
    Log::printf("Processes initialized\n");

    Interrupts::initPic();
    Interrupts::enable();
    Log::printf("Interrupts enabled!\n");

    while (true) {
        asm volatile ("hlt");
    }
}
