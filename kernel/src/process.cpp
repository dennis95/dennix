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

/* kernel/src/process.cpp
 * Process class.
 */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dennix/kernel/elf.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/process.h>
#include <dennix/kernel/terminal.h>

Process* Process::current;
static Process* firstProcess;
static Process* idleProcess;

static pid_t nextPid = 0;

Process::Process() {
    addressSpace = nullptr;
    interruptContext = nullptr;
    prev = nullptr;
    next = nullptr;
    kernelStack = nullptr;
    memset(fd, 0, sizeof(fd));
    rootFd = nullptr;
    cwdFd = nullptr;
    pid = nextPid++;
    contextChanged = false;
    fdInitialized = false;
    terminated = false;
    parent = nullptr;
    children = nullptr;
    numChildren = 0;
    status = 0;
}

Process::~Process() {
    assert(terminated);
    kernelSpace->unmapMemory((vaddr_t) kernelStack, 0x1000);
    free(children);
}

void Process::initialize(FileDescription* rootFd) {
    idleProcess = new Process();
    idleProcess->addressSpace = kernelSpace;
    idleProcess->interruptContext = new InterruptContext();
    idleProcess->rootFd = rootFd;
    current = idleProcess;
    firstProcess = nullptr;
}

void Process::addProcess(Process* process) {
    process->next = firstProcess;
    if (process->next) {
        process->next->prev = process;
    }
    firstProcess = process;

}

int Process::copyArguments(char* const argv[], char* const envp[],
        char**& newArgv, char**& newEnvp, AddressSpace* newAddressSpace) {
    int argc;
    int envc;
    size_t stringSizes = 0;

    for (argc = 0; argv[argc]; argc++) {
        stringSizes += strlen(argv[argc]) + 1;
    }
    for (envc = 0; envp[envc]; envc++) {
        stringSizes += strlen(envp[envc]) + 1;
    }

    stringSizes = ALIGNUP(stringSizes, alignof(char*));

    size_t size = ALIGNUP(stringSizes + (argc + envc + 2) * sizeof(char*),
            0x1000);

    vaddr_t page = newAddressSpace->mapMemory(size, PROT_READ | PROT_WRITE);
    vaddr_t pageMapped = kernelSpace->mapFromOtherAddressSpace(newAddressSpace,
            page, size, PROT_WRITE);

    char* nextString = (char*) pageMapped;
    char** argvMapped = (char**) (pageMapped + stringSizes);
    char** envpMapped = argvMapped + argc + 1;

    for (int i = 0; i < argc; i++) {
        argvMapped[i] = nextString - pageMapped + page;
        nextString = stpcpy(nextString, argv[i]) + 1;
    }
    for (int i = 0; i < envc; i++) {
        envpMapped[i] = nextString - pageMapped + page;
        nextString = stpcpy(nextString, envp[i]) + 1;
    }
    argvMapped[argc] = nullptr;
    envpMapped[envc] = nullptr;

    kernelSpace->unmapPhysical(pageMapped, size);

    newArgv = (char**) (page + stringSizes);
    newEnvp = (char**) (page + stringSizes + (argc + 1) * sizeof(char*));
    return argc;
}

uintptr_t Process::loadELF(uintptr_t elf, AddressSpace* newAddressSpace) {
    ElfHeader* header = (ElfHeader*) elf;

    if (memcmp(header->e_ident, "\x7F""ELF", 4) != 0) {
        errno = ENOEXEC;
        return 0;
    }

    ProgramHeader* programHeader = (ProgramHeader*) (elf + header->e_phoff);

    for (size_t i = 0; i < header->e_phnum; i++) {
        if (programHeader[i].p_type != PT_LOAD) continue;

        vaddr_t loadAddressAligned = programHeader[i].p_paddr & ~0xFFF;
        ptrdiff_t offset = programHeader[i].p_paddr - loadAddressAligned;

        const void* src = (void*) (elf + programHeader[i].p_offset);
        size_t size = ALIGNUP(programHeader[i].p_memsz + offset, 0x1000);

        newAddressSpace->mapMemory(loadAddressAligned, size,
                PROT_READ | PROT_WRITE | PROT_EXEC);
        vaddr_t dest = kernelSpace->mapFromOtherAddressSpace(newAddressSpace,
                loadAddressAligned, size, PROT_WRITE);
        memset((void*) (dest + offset), 0, programHeader[i].p_memsz);
        memcpy((void*) (dest + offset), src, programHeader[i].p_filesz);
        kernelSpace->unmapPhysical(dest, size);
    }

    return (uintptr_t) header->e_entry;
}

InterruptContext* Process::schedule(InterruptContext* context) {
    if (likely(!current->contextChanged)) {
        current->interruptContext = context;
    } else {
        current->contextChanged = false;
    }

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

int Process::execute(FileDescription* descr, char* const argv[],
        char* const envp[]) {
    if (!S_ISREG(descr->vnode->mode)) {
        errno = EACCES;
        return -1;
    }

    // Load the program
    FileVnode* file = (FileVnode*) descr->vnode;
    AddressSpace* newAddressSpace = new AddressSpace();
    uintptr_t entry = loadELF((uintptr_t) file->data, newAddressSpace);
    if (!entry) return -1;

    vaddr_t stack = newAddressSpace->mapMemory(0x1000, PROT_READ | PROT_WRITE);
    kernelStack = (void*) kernelSpace->mapMemory(0x1000,
            PROT_READ | PROT_WRITE);

    InterruptContext* newInterruptContext = (InterruptContext*)
            ((uintptr_t) kernelStack + 0x1000 - sizeof(InterruptContext));

    memset(newInterruptContext, 0, sizeof(InterruptContext));

    char** newArgv;
    char** newEnvp;
    int argc = copyArguments(argv, envp, newArgv, newEnvp, newAddressSpace);

    // Pass argc, argv and envp to the process.
    newInterruptContext->eax = argc;
    newInterruptContext->ebx = (uint32_t) newArgv;
    newInterruptContext->ecx = (uint32_t) newEnvp;
    newInterruptContext->eip = (uint32_t) entry;
    newInterruptContext->cs = 0x1B;
    newInterruptContext->eflags = 0x200; // Interrupt enable
    newInterruptContext->esp = (uint32_t) stack + 0x1000;
    newInterruptContext->ss = 0x23;

    if (!fdInitialized) {
        // Initialize file descriptors
        fd[0] = new FileDescription(&terminal); // stdin
        fd[1] = new FileDescription(&terminal); // stdout
        fd[2] = new FileDescription(&terminal); // stderr

        rootFd = new FileDescription(*idleProcess->rootFd);
        cwdFd = new FileDescription(*rootFd);
        fdInitialized = true;
    }

    Interrupts::disable();
    if (this == current) {
        contextChanged = true;
        kernelSpace->activate();
    }

    if (addressSpace) delete addressSpace;
    addressSpace = newAddressSpace;
    interruptContext = newInterruptContext;
    if (this == current) Interrupts::enable();
    return 0;
}

void Process::exit(int status) {
    Interrupts::disable();
    if (next) {
        next->prev = prev;
    }

    if (prev) {
        prev->next = next;
    }

    if (this == firstProcess) {
        firstProcess = next;
    }

    // Clean up
    delete addressSpace;

    for (size_t i = 0; i < OPEN_MAX; i++) {
        if (fd[i]) delete fd[i];
    }
    delete rootFd;
    delete cwdFd;

    terminated = true;
    this->status = status;
    Interrupts::enable();
}

Process* Process::regfork(int /*flags*/, struct regfork* registers) {
    Process* process = new Process();
    process->parent = this;
    // Add the process to the list of children.
    Process** newChildren = (Process**) reallocarray(children, numChildren + 1,
            sizeof(Process*));
    if (!newChildren) {
        errno = ENOMEM;
        return nullptr;
    }
    children = newChildren;
    children[numChildren++] = process;

    process->kernelStack = (void*) kernelSpace->mapMemory(0x1000,
            PROT_READ | PROT_WRITE);
    process->interruptContext = (InterruptContext*) ((uintptr_t)
            process->kernelStack + 0x1000 - sizeof(InterruptContext));
    process->interruptContext->eax = registers->rf_eax;
    process->interruptContext->ebx = registers->rf_ebx;
    process->interruptContext->ecx = registers->rf_ecx;
    process->interruptContext->edx = registers->rf_edx;
    process->interruptContext->esi = registers->rf_esi;
    process->interruptContext->edi = registers->rf_edi;
    process->interruptContext->ebp = registers->rf_ebp;
    process->interruptContext->eip = registers->rf_eip;
    process->interruptContext->esp = registers->rf_esp;
    // Register that are not controlled by the user
    process->interruptContext->interrupt = 0;
    process->interruptContext->error = 0;
    process->interruptContext->cs = 0x1B;
    process->interruptContext->eflags = 0x200; // Interrupt enable
    process->interruptContext->ss = 0x23;

    // Fork the address space
    process->addressSpace = addressSpace->fork();

    // Fork the file descriptor table
    for (size_t i = 0; i < OPEN_MAX; i++) {
        if (fd[i]) {
            process->fd[i] = new FileDescription(*fd[i]);
        }
    }

    process->rootFd = new FileDescription(*rootFd);
    process->cwdFd = new FileDescription(*cwdFd);
    process->fdInitialized = true;

    addProcess(process);

    return process;
}

int Process::registerFileDescriptor(FileDescription* descr) {
    for (int i = 0; i < OPEN_MAX; i++) {
        if (fd[i] == nullptr) {
            fd[i] = descr;
            return i;
        }
    }

    errno = EMFILE;
    return -1;
}

Process* Process::waitpid(pid_t pid, int flags) {
    if (flags) {
        // Flags are not yet supported
        errno = EINVAL;
        return nullptr;
    }

    for (size_t i = 0; i < numChildren; i++) {
        if (children[i]->pid == pid) {
            Process* result = children[i];
            while (!result->terminated) {
                // Yield until the process terminates.
                sched_yield();
            }

            // Remove the process from the list
            if (i < numChildren - 1) {
                children[i] = children[numChildren - 1];
            }
            children = (Process**) realloc(children,
                    --numChildren * sizeof(Process*));

            return result;
        }
    }

    errno = ECHILD;
    return nullptr;
}
