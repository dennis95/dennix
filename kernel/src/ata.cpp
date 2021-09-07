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

/* kernel/src/ata.cpp
 * ATA driver.
 */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <dennix/seek.h>
#include <dennix/poll.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/ata.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/partition.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/pci.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/portio.h>

#define REGISTER_DATA 0
#define REGISTER_ERROR 1
#define REGISTER_FEATURES 1
#define REGISTER_SECTOR_COUNT 2
#define REGISTER_LBA_LOW 3
#define REGISTER_LBA_MID 4
#define REGISTER_LBA_HIGH 5
#define REGISTER_DEVICE 6
#define REGISTER_STATUS 7
#define REGISTER_COMMAND 7

#define REGISTER_BUSMASTER_COMMAND 0
#define REGISTER_BUSMASTER_STATUS 2
#define REGISTER_BUSMASTER_PRDT 4

#define COMMAND_FLUSH_CACHE 0xE7
#define COMMAND_IDENTIFY_DEVICE 0xEC
#define COMMAND_READ_DMA 0xC8
#define COMMAND_READ_DMA_EXT 0x25
#define COMMAND_WRITE_DMA 0xCA
#define COMMAND_WRITE_DMA_EXT 0x35

#define STATUS_ERROR (1 << 0)
#define STATUS_DATA_REQUEST (1 << 3)
#define STATUS_DEVICE_FAULT (1 << 5)
#define STATUS_BUSY (1 << 7)

#define BUSMASTER_COMMAND_START (1 << 0)
#define BUSMASTER_COMMAND_READ (1 << 3)

#define BUSMASTER_STATUS_ERROR (1 << 1)
#define BUSMASTER_STATUS_INTERRUPT (1 << 2)

static size_t numAtaDevices = 0;
static void onAtaIrq(void* user, const InterruptContext* context);

void AtaController::initialize(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t progIf = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, progIf));
    if (!(progIf & 0x80)) return;

    unsigned int irq1 = Interrupts::isaIrq[14];
    unsigned int irq2 = Interrupts::isaIrq[15];
    if (progIf & 0x5) {
        int irq = Pci::getIrq(bus, device, function);
        if (irq < 0) {
            // We cannot get native PCI interrupts, try switching to ISA mode.
            if ((progIf & 0x3) == 0x3) {
                progIf &= ~0x1;
            }
            if ((progIf & 0xC) == 0xC) {
                progIf &= ~0x4;
            }

            if (progIf & 0x5) {
                Log::printf("ATA controller unsupported: cannot use IRQs\n");
                return;
            }

            uint32_t config = Pci::readConfig(bus, device, function,
                    offsetof(PciHeader, revisionId));
            config = (config & 0xFFFF00FF) | (progIf << 8);
            Pci::writeConfig(bus, device, function,
                    offsetof(PciHeader, revisionId), config);
        }
        if (progIf & 0x1) {
            irq1 = irq;
        }
        if (progIf & 0x4) {
            irq2 = irq;
        }
    }

    uint16_t iobase1 = 0x1F0;
    uint16_t ctrlbase1 = 0x3F6;
    if (progIf & 0x1) {
        uint32_t bar0 = Pci::readConfig(bus, device, function,
                offsetof(PciHeader, bar0));
        iobase1 = bar0 & 0xFFFC;

        uint32_t bar1 = Pci::readConfig(bus, device, function,
                offsetof(PciHeader, bar1));
        ctrlbase1 = bar1 & 0xFFFC;
    }

    uint16_t iobase2 = 0x170;
    uint16_t ctrlbase2 = 0x376;
    if (progIf & 0x4) {
        uint32_t bar2 = Pci::readConfig(bus, device, function,
                offsetof(PciHeader, bar2));
        iobase2 = bar2 & 0xFFFC;

        uint32_t bar3 = Pci::readConfig(bus, device, function,
                offsetof(PciHeader, bar3));
        ctrlbase2 = bar3 & 0xFFFC;
    }

    uint32_t bar4 = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, bar4));
    uint16_t busmasterBase = bar4 & 0xFFFC;

    // Enable PCI busmastering.
    uint32_t cmd = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, command));
    cmd |= (1 << 2);
    Pci::writeConfig(bus, device, function, offsetof(PciHeader, command), cmd);

    paddr_t prdt = PhysicalMemory::popPageFrame32();
    if (!prdt) PANIC("Failed to allocate PRDT");

    vaddr_t prdtMapped = kernelSpace->mapPhysical(prdt, PAGESIZE,
            PROT_READ | PROT_WRITE);
    if (!prdtMapped) PANIC("Failed to map PRDT");

    AtaChannel* channel1 = xnew AtaChannel(iobase1, ctrlbase1, busmasterBase,
            irq1, prdt, prdtMapped);
    AtaChannel* channel2 = xnew AtaChannel(iobase2, ctrlbase2,
            busmasterBase + 8, irq2, prdt + 8, prdtMapped + 8);
    channel1->identifyDevice(false);
    channel1->identifyDevice(true);
    channel2->identifyDevice(false);
    channel2->identifyDevice(true);
}

AtaChannel::AtaChannel(uint16_t iobase, uint16_t ctrlbase,
        uint16_t busmasterBase, unsigned int irq, paddr_t prdPhys,
        vaddr_t prdVirt) {
    mutex = KTHREAD_MUTEX_INITIALIZER;
    this->iobase = iobase;
    this->ctrlbase = ctrlbase;
    this->busmasterBase = busmasterBase;
    this->prdPhys = prdPhys;
    this->prdVirt = prdVirt;

    dmaRegion = PhysicalMemory::popPageFrame32();
    if (!dmaRegion) PANIC("Failed to allocate DMA region");

    dmaMapped = kernelSpace->mapPhysical(dmaRegion, PAGESIZE,
            PROT_READ | PROT_WRITE);
    if (!dmaMapped) PANIC("Failed to map DMA region");

    awaitingInterrupt = false;
    dmaInProgress = false;
    error = false;
    irqHandler.func = onAtaIrq;
    irqHandler.user = this;
    Interrupts::addIrqHandler(irq, &irqHandler);
}

bool AtaChannel::finishDmaTransfer() {
    if (!dmaInProgress) return true;

    while (awaitingInterrupt) {
        sched_yield();
    }

    outb(busmasterBase + REGISTER_BUSMASTER_COMMAND, 0);
    dmaInProgress = false;

    if (error) {
        uint8_t errorValue = inb(iobase + REGISTER_ERROR);
        Log::printf("ATA error 0x%X\n", errorValue);
        return false;
    }
    return true;
}

bool AtaChannel::flushCache(bool secondary) {
    AutoLock lock(&mutex);

    if (!finishDmaTransfer()) return false;

    outb(iobase + REGISTER_DEVICE, 0xE0 | (secondary << 4));
    outb(iobase + REGISTER_COMMAND, COMMAND_FLUSH_CACHE);

    uint8_t status = inb(iobase + REGISTER_STATUS);
    while (status & STATUS_BUSY) {
        status = inb(iobase + REGISTER_STATUS);
    }
    if (status & (STATUS_ERROR | STATUS_DEVICE_FAULT)) return false;

    return true;
}

void AtaChannel::identifyDevice(bool secondary) {
    outb(iobase + REGISTER_DEVICE, 0xE0 | (secondary << 4));
    outb(iobase + REGISTER_COMMAND, COMMAND_IDENTIFY_DEVICE);

    uint8_t status = inb(iobase + REGISTER_STATUS);
    if (status == 0 || status == 0xFF) return;
    while (status & STATUS_BUSY) {
        status = inb(iobase + REGISTER_STATUS);
    }

    while (!(status & (STATUS_DATA_REQUEST | STATUS_ERROR |
            STATUS_DEVICE_FAULT))) {
        status = inb(iobase + REGISTER_STATUS);
    }
    if (status & (STATUS_ERROR | STATUS_DEVICE_FAULT)) return;

    uint16_t data[256];
    for (size_t i = 0; i < 256; i++) {
        data[i] = inw(iobase + REGISTER_DATA);
    }

    if (data[0] & (1 << 15)) return;
    bool lba48Supported = data[83] & (1 << 10);
    uint64_t sectors;

    if (lba48Supported) {
        sectors = data[100] | (data[101] << 16) | ((uint64_t) data[102] << 32) |
                ((uint64_t) data[103] << 48);
    } else {
        sectors = data[60] | (data[61] << 16);
    }

    uint64_t sectorSize = 512;
    if ((data[106] & (1 << 14)) && !(data[106] & (1 << 15))) {
        if (data[106] & (1 << 12)) {
            sectorSize = 2 * (data[117] | (data[118] << 16));
        }
    }

    off_t totalSize;
    if (__builtin_mul_overflow(sectors, sectorSize, &totalSize)) {
        return;
    }

    Reference<AtaDevice> device = xnew AtaDevice(this, secondary, sectors,
            sectorSize, lba48Supported);
    char name[32];
    snprintf(name, sizeof(name), "ata%zu", numAtaDevices++);
    devFS.addDevice(name, device);

    Partition::scanPartitions(device, name, sectorSize);
}

static void onAtaIrq(void* user, const InterruptContext* context) {
    AtaChannel* channel = (AtaChannel*) user;
    channel->onIrq(context);
}

void AtaChannel::onIrq(const InterruptContext* /*context*/) {
    uint8_t busmasterStatus = inb(busmasterBase + REGISTER_BUSMASTER_STATUS);
    if (!(busmasterStatus & BUSMASTER_STATUS_INTERRUPT)) return;

    if (busmasterStatus & BUSMASTER_STATUS_ERROR) {
        error = true;
    }
    // Clear the error and interrupt bits by writing them back.
    outb(busmasterBase + REGISTER_BUSMASTER_STATUS, busmasterStatus);
    uint8_t status = inb(iobase + REGISTER_STATUS);
    if (status & (STATUS_ERROR | STATUS_DEVICE_FAULT)) {
        error = true;
    }
    awaitingInterrupt = false;
}

bool AtaChannel::readSectors(char* buffer, size_t sectorCount, uint64_t lba,
        bool secondary, uint64_t sectorSize) {
    AutoLock lock(&mutex);
    if (!finishDmaTransfer()) return false;

    bool useLba48 = setSectors(sectorCount, lba, secondary);

    uint32_t* prd = (uint32_t*) prdVirt;
    prd[0] = dmaRegion;
    prd[1] = (sectorCount * sectorSize) | (1U << 31);
    outl(busmasterBase + REGISTER_BUSMASTER_PRDT, prdPhys);

    outb(busmasterBase + REGISTER_BUSMASTER_STATUS,
            BUSMASTER_STATUS_ERROR | BUSMASTER_STATUS_INTERRUPT);

    outb(busmasterBase + REGISTER_BUSMASTER_COMMAND,
            BUSMASTER_COMMAND_READ);

    uint8_t command = useLba48 ? COMMAND_READ_DMA_EXT : COMMAND_READ_DMA;
    outb(iobase + REGISTER_COMMAND, command);

    awaitingInterrupt = true;
    dmaInProgress = true;
    error = false;
    outb(busmasterBase + REGISTER_BUSMASTER_COMMAND,
            BUSMASTER_COMMAND_START | BUSMASTER_COMMAND_READ);
    if (!finishDmaTransfer()) return false;
    memcpy(buffer, (void*) dmaMapped, sectorSize * sectorCount);
    return true;
}

bool AtaChannel::setSectors(size_t sectorCount, uint64_t lba, bool secondary) {
    if (lba > 0xFFFFFFF || sectorCount > 256) {
        // We need to use lba48 for this.
        uint16_t count = sectorCount == 65536 ? 0 : sectorCount;

        outb(iobase + REGISTER_DEVICE, 0xE0 | (secondary << 4));
        outb(iobase + REGISTER_SECTOR_COUNT, (count >> 8) & 0xFF);
        outb(iobase + REGISTER_LBA_LOW, (lba >> 24) & 0xFF);
        outb(iobase + REGISTER_LBA_MID, (lba >> 32) & 0xFF);
        outb(iobase + REGISTER_LBA_HIGH, (lba >> 40) & 0xFF);
        outb(iobase + REGISTER_SECTOR_COUNT, count & 0xFF);
        outb(iobase + REGISTER_LBA_LOW, lba & 0xFF);
        outb(iobase + REGISTER_LBA_MID, (lba >> 8) & 0xFF);
        outb(iobase + REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);
        return true;
    } else {
        uint8_t count = sectorCount == 256 ? 0 : sectorCount;
        outb(iobase + REGISTER_DEVICE, 0xE0 | (secondary << 4) |
                ((lba >> 24) & 0x0F));
        outb(iobase + REGISTER_SECTOR_COUNT, count);
        outb(iobase + REGISTER_LBA_LOW, lba & 0xFF);
        outb(iobase + REGISTER_LBA_MID, (lba >> 8) & 0xFF);
        outb(iobase + REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);
        return false;
    }
}

bool AtaChannel::writeSectors(const char* buffer, size_t sectorCount,
        uint64_t lba, bool secondary, uint64_t sectorSize) {
    AutoLock lock(&mutex);
    if (!finishDmaTransfer()) return false;

    bool useLba48 = setSectors(sectorCount, lba, secondary);
    memcpy((void*) dmaMapped, buffer, sectorSize * sectorCount);

    uint32_t* prd = (uint32_t*) prdVirt;
    prd[0] = dmaRegion;
    prd[1] = (sectorCount * sectorSize) | (1U << 31);
    outl(busmasterBase + REGISTER_BUSMASTER_PRDT, prdPhys);

    outb(busmasterBase + REGISTER_BUSMASTER_STATUS,
            BUSMASTER_STATUS_ERROR | BUSMASTER_STATUS_INTERRUPT);

    outb(busmasterBase + REGISTER_BUSMASTER_COMMAND, 0);

    uint8_t command = useLba48 ? COMMAND_WRITE_DMA_EXT : COMMAND_WRITE_DMA;
    outb(iobase + REGISTER_COMMAND, command);

    awaitingInterrupt = true;
    dmaInProgress = true;
    error = false;
    outb(busmasterBase + REGISTER_BUSMASTER_COMMAND,
            BUSMASTER_COMMAND_START);
    // The transfer will be finished asynchronously.
    return true;
}

AtaDevice::AtaDevice(AtaChannel* channel, bool secondary, uint64_t sectors,
        uint64_t sectorSize, bool lba48Supported) : BlockCacheDevice(0644,
        DevFS::dev) {
    this->channel = channel;
    this->secondary = secondary;
    this->sectors = sectors;
    this->sectorSize = sectorSize;
    this->lba48Supported = lba48Supported;

    stats.st_size = sectors * sectorSize;
    stats.st_blksize = sectorSize;
}

off_t AtaDevice::lseek(off_t offset, int whence) {
    AutoLock lock(&mutex);
    off_t base;

    if (whence == SEEK_SET || whence == SEEK_CUR) {
        base = 0;
    } else if (whence == SEEK_END) {
        base = stats.st_size;
    } else {
        errno = EINVAL;
        return -1;
    }

    off_t result;
    if (__builtin_add_overflow(base, offset, &result) || result < 0 ||
            result > stats.st_size) {
        errno = EINVAL;
        return -1;
    }

    return result;
}

short AtaDevice::poll() {
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

bool AtaDevice::readUncached(void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    char* buf = (char*) buffer;

    assert(offset % sectorSize == 0);
    assert(size % sectorSize == 0);
    assert(size <= PAGESIZE);
    assert(offset < stats.st_size);

    size_t sectors = size / sectorSize;
    uint64_t lba = offset / sectorSize;
    if (!channel->readSectors(buf, sectors, lba, secondary, sectorSize)) {
        errno = EIO;
        return false;
    }

    return true;
}

bool AtaDevice::writeUncached(const void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    const char* buf = (const char*) buffer;

    assert(offset % sectorSize == 0);
    assert(size % sectorSize == 0);
    assert(size <= PAGESIZE);
    assert(offset < stats.st_size);

    size_t sectors = size / sectorSize;
    uint64_t lba = offset / sectorSize;
    if (!channel->writeSectors(buf, sectors, lba, secondary, sectorSize)) {
        errno = EIO;
        return false;
    }

    return true;
}

int AtaDevice::sync(int /*flags*/) {
    if (!channel->flushCache(secondary)) {
        errno = EIO;
        return -1;
    }
    return 0;
}
