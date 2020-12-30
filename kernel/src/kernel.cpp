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

/* kernel/src/kernel.cpp
 * The kernel's main function.
 */

#include <assert.h>
#include <string.h>
#include <dennix/fcntl.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/directory.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/initrd.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/pci.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/pit.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/ps2.h>
#include <dennix/kernel/rtc.h>
#include <dennix/kernel/terminal.h>

#ifndef DENNIX_VERSION
#  define DENNIX_VERSION ""
#endif

static Reference<DirectoryVnode> loadInitrd(multiboot_info* multiboot);

static multiboot_info multiboot;

extern "C" void kmain(uint32_t /*magic*/, paddr_t multibootAddress) {
    AddressSpace::initialize();

    // Copy the multiboot structure. This cannot fail because we just freed some
    // memory.
    vaddr_t multibootMapping;
    size_t mapSize;
    const multiboot_info* multibootMapped = (const multiboot_info*)
            kernelSpace->mapUnaligned(multibootAddress, sizeof(multiboot_info),
            PROT_READ, multibootMapping, mapSize);
    memcpy(&multiboot, multibootMapped, sizeof(multiboot_info));
    kernelSpace->unmapPhysical(multibootMapping, mapSize);

    Log::earlyInitialize(&multiboot);
    // Kernel panic works after this point.

    PhysicalMemory::initialize(&multiboot);

    Log::initialize();
    Log::printf("Welcome to Dennix " DENNIX_VERSION "\n");
    Log::printf("Initializing PS/2 Controller...\n");
    PS2::initialize();

    Thread::initializeIdleThread();
    Interrupts::initPic();
    Log::printf("Initializing RTC and PIT...\n");
    Rtc::initialize();
    Pit::initialize();

    Log::printf("Scanning for PCI devices...\n");
    Pci::scanForDevices();

    Log::printf("Enabling interrupts...\n");
    Interrupts::enable();

    // Load the initrd.
    Log::printf("Loading Initrd...\n");
    Reference<DirectoryVnode> rootDir = loadInitrd(&multiboot);
    if (!rootDir) PANIC("Could not load initrd");
    Reference<FileDescription> rootFd = xnew FileDescription(rootDir, O_SEARCH);
    Process::current()->rootFd = rootFd;

    Devices::initialize(rootDir);
    rootDir->mkdir("tmp", 0777);
    rootDir->mkdir("run", 0755);

    Log::printf("Starting init process...\n");
    Reference<Vnode> program = resolvePath(rootDir, "/sbin/init");
    if (!program) PANIC("No init program found");

    Process* initProcess = xnew Process();
    const char* argv[] = { "init", nullptr };
    const char* envp[] = { nullptr };
    if (initProcess->execute(program, (char**) argv, (char**) envp) < 0) {
        PANIC("Failed to start init process");
    }
    program = nullptr;
    if (!Process::addProcess(initProcess)) {
        PANIC("Failed to start init process");
    }
    assert(initProcess->pid == 1);
    Process::initProcess = initProcess;

    initProcess->controllingTerminal = terminal;
    Reference<FileDescription> descr = xnew FileDescription(terminal, O_RDWR);
    initProcess->addFileDescriptor(descr, 0); // stdin
    initProcess->addFileDescriptor(descr, 0); // stdout
    initProcess->addFileDescriptor(descr, 0); // stderr

    initProcess->rootFd = rootFd;
    initProcess->cwdFd = rootFd;
    Thread::addThread(&initProcess->mainThread);

    while (true) {
        asm volatile ("hlt");
    }
}

static Reference<DirectoryVnode> loadInitrd(multiboot_info* multiboot) {
    Reference<DirectoryVnode> root;
    vaddr_t mapping;
    size_t mapSize;
    const multiboot_mod_list* modules = (const multiboot_mod_list*)
            kernelSpace->mapUnaligned(multiboot->mods_addr,
            multiboot->mods_count * sizeof(multiboot_mod_list), PROT_READ,
            mapping, mapSize);
    if (!modules) PANIC("Failed to map multiboot modules");

    for (size_t i = 0; i < multiboot->mods_count; i++) {
        // Multiboot modules are guaranteed to be page aligned.
        size_t size = ALIGNUP(modules[i].mod_end - modules[i].mod_start,
                PAGESIZE);
        vaddr_t initrd = kernelSpace->mapPhysical(modules[i].mod_start, size,
                PROT_READ);
        if (!initrd) PANIC("Failed to map initrd");
        root = Initrd::loadInitrd(initrd);
        kernelSpace->unmapPhysical(initrd, size);

        if (root->childCount) break;
    }
    kernelSpace->unmapPhysical(mapping, mapSize);

    return root;
}
