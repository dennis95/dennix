/* Copyright (c) 2016, 2017, 2018, 2019 Dennis Wölfing
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
#include <dennix/kernel/directory.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/initrd.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/panic.h>
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

    // Copy the multiboot structure.
    multiboot_info* multibootMapped = (multiboot_info*)
            kernelSpace->mapPhysical(multibootAddress, PAGESIZE, PROT_READ);
    memcpy(&multiboot, multibootMapped, sizeof(multiboot_info));
    kernelSpace->unmapPhysical((vaddr_t) multibootMapped, PAGESIZE);

    PhysicalMemory::initialize(&multiboot);

    Log::initialize(&multiboot);
    Log::printf("Welcome to Dennix " DENNIX_VERSION "\n");
    Log::printf("Initializing PS/2 Controller...\n");
    PS2::initialize();

    Thread::initializeIdleThread();
    Interrupts::initPic();
    Log::printf("Initializing RTC and PIT...\n");
    Rtc::initialize();
    Pit::initialize();

    Log::printf("Enabling interrupts...\n");
    Interrupts::enable();

    // Load the initrd.
    Log::printf("Loading Initrd...\n");
    Reference<DirectoryVnode> rootDir = loadInitrd(&multiboot);
    Reference<FileDescription> rootFd = new FileDescription(rootDir, O_SEARCH);
    Process::current()->rootFd = rootFd;

    Log::printf("Starting init process...\n");
    Reference<Vnode> program = resolvePath(rootDir, "/sbin/init");
    if (!program) PANIC("No init program found");

    Process* initProcess = new Process();
    const char* argv[] = { "init", nullptr };
    const char* envp[] = { nullptr };
    initProcess->execute(program, (char**) argv, (char**) envp);
    program = nullptr;
    Process::addProcess(initProcess);
    assert(initProcess->pid == 1);
    Process::initProcess = initProcess;

    initProcess->controllingTerminal = terminal;
    Reference<FileDescription> descr = new FileDescription(terminal, O_RDWR);
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

    paddr_t modulesAligned = multiboot->mods_addr & ~PAGE_MISALIGN;
    ptrdiff_t offset = multiboot->mods_addr - modulesAligned;
    size_t mappedSize = ALIGNUP(offset +
            multiboot->mods_count * sizeof(multiboot_mod_list), PAGESIZE);

    vaddr_t modulesPage = kernelSpace->mapPhysical(modulesAligned, mappedSize,
            PROT_READ);

    const multiboot_mod_list* modules = (multiboot_mod_list*)
            (modulesPage + offset);
    for (size_t i = 0; i < multiboot->mods_count; i++) {
        size_t size = ALIGNUP(modules[i].mod_end - modules[i].mod_start,
                PAGESIZE);
        vaddr_t initrd = kernelSpace->mapPhysical(modules[i].mod_start, size,
                PROT_READ);
        root = Initrd::loadInitrd(initrd);
        kernelSpace->unmapPhysical(initrd, size);

        if (root->childCount) break;
    }
    kernelSpace->unmapPhysical((vaddr_t) modulesPage, mappedSize);

    return root;
}
