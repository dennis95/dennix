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

/* kernel/include/dennix/kernel/process.h
 * Process class.
 */

#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/filedescription.h>
#include <dennix/kernel/interrupts.h>

class Process {
public:
    Process();
    void exit(int status);
private:
    InterruptContext* interruptContext;
    Process* prev;
    Process* next;
    void* stack;
    void* kernelStack;
public:
    AddressSpace* addressSpace;
    FileDescription* fd[20];
public:
    static void initialize();
    static Process* loadELF(vaddr_t elf);
    static InterruptContext* schedule(InterruptContext* context);
    static Process* startProcess(void* entry, AddressSpace* addressSpace);
    static Process* current;
};

void setKernelStack(uintptr_t stack);

#endif