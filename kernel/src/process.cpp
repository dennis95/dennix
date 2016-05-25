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

/* kernel/src/process.cpp
 * Process class.
 */

#include <string.h>
#include <dennix/kernel/elf.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/terminal.h>

Process* Process::current;
static Process* firstProcess;
static Process* idleProcess;

Process::Process() {
    addressSpace = kernelSpace;
    interruptContext = nullptr;
    prev = nullptr;
    next = nullptr;
    stack = nullptr;
    kernelStack = nullptr;
    memset(fd, 0, sizeof(fd));
}

void Process::initialize() {
    idleProcess = new Process();
    idleProcess->interruptContext = new InterruptContext();
    current = idleProcess;
    firstProcess = nullptr;
}

Process* Process::loadELF(vaddr_t elf) {
    ElfHeader* header = (ElfHeader*) elf;
    ProgramHeader* programHeader = (ProgramHeader*) (elf + header->e_phoff);

    AddressSpace* addressSpace = kernelSpace->fork();

    for (size_t i = 0; i < header->e_phnum; i++) {
        if (programHeader[i].p_type != PT_LOAD) continue;

        vaddr_t loadAddressAligned = programHeader[i].p_paddr & ~0xFFF;
        ptrdiff_t offset = programHeader[i].p_paddr - loadAddressAligned;

        const void* src = (void*) (elf + programHeader[i].p_offset);
        size_t nPages = ALIGNUP(programHeader[i].p_memsz + offset, 0x1000)
                / 0x1000;
        paddr_t destPhys[nPages + 1];
        for (size_t j = 0; j < nPages; j++) {
            destPhys[j] = PhysicalMemory::popPageFrame();
        }
        destPhys[nPages] = 0;

        vaddr_t dest =
                kernelSpace->mapRange(destPhys, PAGE_PRESENT | PAGE_WRITABLE);
        memset((void*) (dest + offset), 0, programHeader[i].p_memsz);
        memcpy((void*) (dest + offset), src, programHeader[i].p_filesz);
        kernelSpace->unmapRange(dest, nPages);

        addressSpace->mapRangeAt(loadAddressAligned, destPhys,
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    return startProcess((void*) header->e_entry, addressSpace);
}

InterruptContext* Process::schedule(InterruptContext* context) {
    current->interruptContext = context;

    if (current->next) {
        current = current->next;
    } else {
        if (firstProcess) {
            current = firstProcess;
        } else {
            current = idleProcess;
        }
    }

    setKernelStack((uintptr_t) current->kernelStack + 0x1000);

    current->addressSpace->activate();
    return current->interruptContext;
}

Process* Process::startProcess(void* entry, AddressSpace* addressSpace) {
    Process* process = new Process();
    process->stack = (void*) addressSpace->allocate(1);
    process->kernelStack = (void*) kernelSpace->allocate(1);

    process->interruptContext = (InterruptContext*) ((uintptr_t)
            process->kernelStack + 0x1000 - sizeof(InterruptContext));

    process->interruptContext->eax = 0;
    process->interruptContext->ebx = 0;
    process->interruptContext->ecx = 0;
    process->interruptContext->edx = 0;
    process->interruptContext->esi = 0;
    process->interruptContext->edi = 0;
    process->interruptContext->ebp = 0;
    process->interruptContext->interrupt = 0;
    process->interruptContext->error = 0;
    process->interruptContext->eip = (uint32_t) entry;
    process->interruptContext->cs = 0x1B;
    process->interruptContext->eflags = 0x200; // Interrupt enable
    process->interruptContext->esp = (uint32_t) process->stack + 0x1000;
    process->interruptContext->ss = 0x23;

    process->addressSpace = addressSpace;

    // Initialize file descriptors
    process->fd[1] = new FileDescription(&terminal); // stdout

    process->next = firstProcess;
    if (process->next) {
        process->next->prev = process;
    }
    firstProcess = process;

    return process;
}

void Process::exit(int status) {
    if (next) {
        next->prev = prev;
    }

    if (prev) {
        prev->next = next;
    }

    if (this == firstProcess) {
        firstProcess = next;
    }

    // TODO: We currently leak the processes memory.
    Log::printf("Process exited with status %u\n", status);
}
