/* Copyright (c) 2021, 2022 Dennis Wölfing
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

/* kernel/src/acpi.cpp
 * ACPI tables.
 */

#include <string.h>
#include <dennix/kernel/acpi.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/hpet.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/multiboot2.h>
#include <dennix/kernel/panic.h>

struct Rsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt;
} PACKED;

struct IsdtHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} PACKED;

struct Rsdt {
    IsdtHeader header;
    uint32_t tables[];
} PACKED;

struct Madt {
    IsdtHeader header;
    uint32_t localApicAddress;
    uint32_t flags;
    char entries[];
} PACKED;

struct MadtEntryHeader {
    uint8_t type;
    uint8_t length;
} PACKED;

struct MadtIoApic {
    MadtEntryHeader header;
    uint8_t ioApicId;
    uint8_t reserved;
    uint32_t ioApicAddress;
    uint32_t globalSystemInterruptBase;
} PACKED;

struct MadtInterruptSourceOverride {
    MadtEntryHeader header;
    uint8_t busSource;
    uint8_t irqSource;
    uint32_t globalSystemInterrupt;
    uint16_t flags;
} PACKED;

struct GenericAddressStructure {
    uint8_t addressSpace;
    uint8_t bitWidth;
    uint8_t bitOffset;
    uint8_t accessSize;
    uint64_t address;
} PACKED;

struct HpetTable {
    IsdtHeader header;
    uint32_t eventTimerBlockId;
    GenericAddressStructure baseAddress;
    uint8_t hpetNumber;
    uint16_t minimumClockTick;
    uint8_t pageProtection;
} PACKED;

static paddr_t getRsdt(const multiboot_info* multiboot);
static void scanHpet(paddr_t address, size_t length);
static void scanMadt(paddr_t address, size_t length);
static bool verifyTable(const void* table, size_t size);

void Acpi::initialize(const multiboot_info* multiboot) {
    paddr_t rsdtAddress = getRsdt(multiboot);
    if (!rsdtAddress) return;

    vaddr_t mapping;
    size_t mapSize;
    const IsdtHeader* header = (const IsdtHeader*) kernelSpace->mapUnaligned(
            rsdtAddress, sizeof(IsdtHeader), PROT_READ, mapping, mapSize);
    if (!header) PANIC("Failed to map RSDT");

    size_t rsdtSize = header->length;
    kernelSpace->unmapPhysical(mapping, mapSize);

    const Rsdt* rsdt = (const Rsdt*) kernelSpace->mapUnaligned(rsdtAddress,
            rsdtSize, PROT_READ, mapping, mapSize);
    if (!rsdt) PANIC("Failed to map RSDT");

    if (!verifyTable(rsdt, rsdtSize)) {
        Log::printf("RSDT verification failed\n");
        kernelSpace->unmapPhysical(mapping, mapSize);
        return;
    }

    size_t numTables = (rsdtSize - sizeof(IsdtHeader)) / sizeof(uint32_t);

    paddr_t hpet = 0;
    size_t hpetLength;

    paddr_t madt = 0;
    size_t madtLength;

    for (size_t i = 0; i < numTables; i++) {
        vaddr_t tableMapping;
        size_t tableMapSize;

        const IsdtHeader* header = (const IsdtHeader*)
                kernelSpace->mapUnaligned(rsdt->tables[i], sizeof(IsdtHeader),
                PROT_READ, tableMapping, tableMapSize);
        if (!header) PANIC("Failed to map ACPI table");

        if (memcmp(header->signature, "APIC", 4) == 0) {
            madt = rsdt->tables[i];
            madtLength = header->length;
        } else if (memcmp(header->signature, "HPET", 4) == 0) {
            hpet = rsdt->tables[i];
            hpetLength = header->length;
        }

        kernelSpace->unmapPhysical(tableMapping, tableMapSize);
    }

    if (madt) {
        scanMadt(madt, madtLength);
    }

    if (hpet) {
        scanHpet(hpet, hpetLength);
    }

    kernelSpace->unmapPhysical(mapping, mapSize);
}

static paddr_t getRsdt(const multiboot_info* multiboot) {
    uintptr_t p = (uintptr_t) multiboot + 8;

    while (true) {
        const multiboot_tag* tag = (const multiboot_tag*) p;
        if (tag->type == MULTIBOOT_TAG_TYPE_END) return 0;

        if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD ||
                tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) {
            const multiboot_tag_acpi* acpiTag = (const multiboot_tag_acpi*) tag;
            const Rsdp* rsdp = (const Rsdp*) acpiTag->rsdp;
            return rsdp->rsdt;
        }

        p = ALIGNUP(p + tag->size, 8);
    }
}

static void scanHpet(paddr_t address, size_t length) {
    vaddr_t mapping;
    size_t mapSize;

    const HpetTable* hpet = (const HpetTable*) kernelSpace->mapUnaligned(
            address, length, PROT_READ, mapping, mapSize);
    if (!hpet) PANIC("Failed to map HPET");

    if (!verifyTable(hpet, length)) {
        Log::printf("HPET verification failed");
        kernelSpace->unmapPhysical(mapping, mapSize);
        return;
    }

    Hpet::initialize(hpet->baseAddress.address);
    kernelSpace->unmapPhysical(mapping, mapSize);
}

static void scanMadt(paddr_t address, size_t length) {
    vaddr_t mapping;
    size_t mapSize;

    const Madt* madt = (const Madt*) kernelSpace->mapUnaligned(address,
            length, PROT_READ, mapping, mapSize);
    if (!madt) PANIC("Failed to map MADT");

    if (!verifyTable(madt, length)) {
        Log::printf("MADT verification failed\n");
        kernelSpace->unmapPhysical(mapping, mapSize);
        return;
    }

    Interrupts::initApic();

    uintptr_t p = (uintptr_t) madt->entries;
    while (p < (uintptr_t) madt + madt->header.length) {
        const MadtEntryHeader* header = (const MadtEntryHeader*) p;

        if (header->type == 1) {
            const MadtIoApic* entry = (const MadtIoApic*) header;
            Interrupts::initIoApic(entry->ioApicAddress,
                    entry->globalSystemInterruptBase);
        }

        if (header->type == 2) {
            const MadtInterruptSourceOverride* entry =
                    (const MadtInterruptSourceOverride*) header;
            Interrupts::isaIrq[entry->irqSource] = entry->globalSystemInterrupt;
        }

        p += header->length;
    }

    kernelSpace->unmapPhysical(mapping, mapSize);
}

static bool verifyTable(const void* table, size_t size) {
    uint8_t sum = 0;
    const uint8_t* p = (const uint8_t*) table;
    for (size_t i = 0; i < size; i++) {
        sum += p[i];
    }
    return sum == 0;
}
