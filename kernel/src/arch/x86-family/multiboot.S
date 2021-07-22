/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/src/arch/x86-family/multiboot.S
 * Multiboot2 header.
 */

#define MULTIBOOT_MAGIC 0xE85250D6
#define MULTIBOOT_ARCH 0
#define MULTIBOOT_HEADER_LENGTH (multibootEnd - multibootStart)
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_ARCH + \
        MULTIBOOT_HEADER_LENGTH)

# Choose preferred video settings. The bootloader is still allowed to choose a
# different video mode.
#define PREFERRED_WIDTH 1024
#define PREFERRED_HEIGHT 768

.section multiboot
.align 8
multibootStart:
.long MULTIBOOT_MAGIC
.long MULTIBOOT_ARCH
.long MULTIBOOT_HEADER_LENGTH
.long MULTIBOOT_CHECKSUM

# Request information from the bootloader.
.align 8
.word 1
.word 0
.long 28 # Size
.long 3 # Modules
.long 6 # Memory map
.long 8 # Framebuffer info
.long 14 # RSDP (ACPI 1.0)
.long 15 # RSDP (ACPI 2.0+)

# Request a framebuffer.
.align 8
.word 5
.word 0
.long 20
.long PREFERRED_WIDTH
.long PREFERRED_HEIGHT
.long 32 # bpp

# Request modules to be page aligned.
.align 8
.word 6
.word 0
.long 8

# End of tags.
.align 8
.word 0
.word 0
.long 8
multibootEnd:
