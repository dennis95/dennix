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

/* kernel/linker.ld
 * Linker script that tells how the kernel will be loaded into memory.
 */

ENTRY(_start);

/* This is the address where the kernel space starts. */
KERNEL_VIRTUAL = 0xC0000000;

SECTIONS {
    . = 0x100000;

    bootstrapBegin = .;

    bootstrap_text : {
        *(multiboot)
        *(bootstrap_text)
    }

    bootstrap_bss ALIGN(4096) : {
        *(bootstrap_bss)
    }

    bootstrapEnd = ALIGN(4096);
    numBootstrapPages = (bootstrapEnd - bootstrapBegin) / 0x1000;

    . += KERNEL_VIRTUAL;

    kernelVirtualBegin = ALIGN(4096);
    kernelPhysicalBegin = kernelVirtualBegin - KERNEL_VIRTUAL;

    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VIRTUAL) {
        *(.text)
    }

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VIRTUAL) {
        *(.rodata)
    }

    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VIRTUAL) {
        *(.data)
    }

    .bss ALIGN(4096) : AT(ADDR(.bss) - KERNEL_VIRTUAL) {
        *(.bss)
    }

    kernelVirtualEnd = ALIGN(4096);
    kernelPhysicalEnd = kernelVirtualEnd - KERNEL_VIRTUAL;
    numKernelPages = (kernelVirtualEnd - kernelVirtualBegin) / 0x1000;
}
