/* Copyright (c) 2016, 2017 Dennis Wölfing
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
#include <dennix/kernel/directory.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/initrd.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/pit.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/ps2.h>

#ifndef DENNIX_VERSION
#  define DENNIX_VERSION ""
#endif

static DirectoryVnode* loadInitrd(multiboot_info* multiboot);

static multiboot_info multiboot;

extern "C" void kmain(uint32_t /*magic*/, paddr_t multibootAddress) {
    Log::printf("Welcome to Dennix " DENNIX_VERSION "\n");
    Log::printf("Initializing Address space...\n");
    AddressSpace::initialize();

    // Copy the multiboot structure.
    multiboot_info* multibootMapped = (multiboot_info*)
            kernelSpace->mapPhysical(multibootAddress, 0x1000, PROT_READ);
    memcpy(&multiboot, multibootMapped, sizeof(multiboot_info));
    kernelSpace->unmapPhysical((vaddr_t) multibootMapped, 0x1000);

    Log::printf("Initializing Physical Memory...\n");
    PhysicalMemory::initialize(&multiboot);

    Log::printf("Initializing PS/2 Controller...\n");
    PS2::initialize();

    // Load the initrd.
    Log::printf("Loading Initrd...\n");
    DirectoryVnode* rootDir = loadInitrd(&multiboot);
    FileDescription* rootFd = new FileDescription(rootDir);

    Log::printf("Initializing processes...\n");
    Process::initialize(rootFd);
    FileVnode* program = (FileVnode*) resolvePath(rootDir, "/bin/sh");
    if (program) {
        Process* newProcess = new Process();
        const char* argv[] = { "/bin/sh", nullptr };
        const char* envp[] = { "PATH=/bin", nullptr };
        newProcess->execute(program, (char**) argv, (char**) envp);
        Process::addProcess(newProcess);
    }

    Log::printf("Enabling interrupts...\n");
    Interrupts::initPic();
    Pit::initialize();
    Log::printf("Initialization completed.\n");
    Interrupts::enable();

    while (true) {
        asm volatile ("hlt");
    }
}

static DirectoryVnode* loadInitrd(multiboot_info* multiboot) {
    DirectoryVnode* root = nullptr;

    paddr_t modulesAligned = multiboot->mods_addr & ~0xFFF;
    ptrdiff_t offset = multiboot->mods_addr - modulesAligned;
    size_t mappedSize = ALIGNUP(offset +
            multiboot->mods_count * sizeof(multiboot_mod_list), 0x1000);

    vaddr_t modulesPage = kernelSpace->mapPhysical(modulesAligned, mappedSize,
            PROT_READ);

    const multiboot_mod_list* modules = (multiboot_mod_list*)
            (modulesPage + offset);
    for (size_t i = 0; i < multiboot->mods_count; i++) {
        size_t size = ALIGNUP(modules[i].mod_end - modules[i].mod_start,
                0x1000);
        vaddr_t initrd = kernelSpace->mapPhysical(modules[i].mod_start, size,
                PROT_READ);
        root = Initrd::loadInitrd(initrd);
        kernelSpace->unmapPhysical(initrd, size);

        if (root->childCount) break;
    }
    kernelSpace->unmapPhysical((vaddr_t) modulesPage, mappedSize);

    return root;
}
