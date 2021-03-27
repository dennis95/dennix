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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dennix/seek.h>
#include <dennix/poll.h>
#include <dennix/kernel/ata.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/partition.h>
#include <dennix/kernel/pci.h>
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

#define REGISTER_BUSMASTER_STATUS 2

#define COMMAND_FLUSH_CACHE 0xE7
#define COMMAND_IDENTIFY_DEVICE 0xEC
#define COMMAND_READ_SECTORS 0x20
#define COMMAND_READ_SECTORS_EXT 0x24
#define COMMAND_WRITE_SECTORS 0x30
#define COMMAND_WRITE_SECTORS_EXT 0x34

#define STATUS_ERROR (1 << 0)
#define STATUS_DATA_REQUEST (1 << 3)
#define STATUS_DEVICE_FAULT (1 << 5)
#define STATUS_BUSY (1 << 7)

static size_t numAtaDevices = 0;
static void onAtaIrq(void* user, const InterruptContext* context);

void AtaController::initialize(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t progIf = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, progIf));
    if (!(progIf & 0x80)) return;

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

    unsigned int irq1 = 14;
    unsigned int irq2 = 15;
    if (progIf & 0x5) {
        uint8_t interruptLine = Pci::readConfig(bus, device, function,
                offsetof(PciHeader, interruptLine));
        if (progIf & 0x1) {
            irq1 = interruptLine;
        }
        if (progIf & 0x4) {
            irq2 = interruptLine;
        }
    }

    uint32_t bar4 = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, bar4));
    uint16_t busmasterBase = bar4 & 0xFFFC;

    AtaChannel* channel1 = xnew AtaChannel(iobase1, ctrlbase1, busmasterBase,
            irq1);
    AtaChannel* channel2 = xnew AtaChannel(iobase2, ctrlbase2,
            busmasterBase + 8, irq2);
    channel1->identifyDevice(false);
    channel1->identifyDevice(true);
    channel2->identifyDevice(false);
    channel2->identifyDevice(true);
}

AtaChannel::AtaChannel(uint16_t iobase, uint16_t ctrlbase,
        uint16_t busmasterBase, unsigned int irq) {
    mutex = KTHREAD_MUTEX_INITIALIZER;
    this->iobase = iobase;
    this->ctrlbase = ctrlbase;
    this->busmasterBase = busmasterBase;

    irqHandler.func = onAtaIrq;
    irqHandler.user = this;
    Interrupts::addIrqHandler(irq, &irqHandler);
}

bool AtaChannel::flushCache(bool secondary) {
    AutoLock lock(&mutex);

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
    devFS->addDevice(name, device);

    Partition::scanPartitions(device, name, sectorSize);
}

static void onAtaIrq(void* user, const InterruptContext* context) {
    AtaChannel* channel = (AtaChannel*) user;
    channel->onIrq(context);
}

void AtaChannel::onIrq(const InterruptContext* /*context*/) {
    uint8_t busmasterStatus = inb(busmasterBase + REGISTER_BUSMASTER_STATUS);
    if (!(busmasterStatus & (1 << 2))) return;
    // Clear the interrupt bit by writing it back.
    outb(busmasterBase + 2, busmasterStatus);
    inb(iobase + REGISTER_STATUS);
}

bool AtaChannel::readSectors(char* buffer, size_t sectorCount, uint64_t lba,
        bool secondary, uint64_t sectorSize) {
    AutoLock lock(&mutex);

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
        outb(iobase + REGISTER_COMMAND, COMMAND_READ_SECTORS_EXT);
    } else {
        uint8_t count = sectorCount == 256 ? 0 : sectorCount;
        outb(iobase + REGISTER_DEVICE, 0xE0 | (secondary << 4) |
                ((lba >> 24) & 0x0F));
        outb(iobase + REGISTER_SECTOR_COUNT, count);
        outb(iobase + REGISTER_LBA_LOW, lba & 0xFF);
        outb(iobase + REGISTER_LBA_MID, (lba >> 8) & 0xFF);
        outb(iobase + REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);
        outb(iobase + REGISTER_COMMAND, COMMAND_READ_SECTORS);
    }

    for (size_t i = 0; i < sectorCount; i++) {
        uint8_t status = inb(iobase + REGISTER_STATUS);
        while (status & STATUS_BUSY) {
            status = inb(iobase + REGISTER_STATUS);
        }
        while (!(status & (STATUS_DATA_REQUEST | STATUS_ERROR |
                STATUS_DEVICE_FAULT))) {
            status = inb(iobase + REGISTER_STATUS);
        }
        if (status & (STATUS_ERROR | STATUS_DEVICE_FAULT)) return false;

        for (size_t j = 0; j < sectorSize / 2; j++) {
            uint16_t word = inw(iobase + REGISTER_DATA);
            buffer[2 * j] = word & 0xFF;
            buffer[2 * j + 1] = (word >> 8) & 0xFF;
        }

        buffer += sectorSize;
    }

    return true;
}

bool AtaChannel::writeSectors(const char* buffer, size_t sectorCount,
        uint64_t lba, bool secondary, uint64_t sectorSize) {
    AutoLock lock(&mutex);

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
        outb(iobase + REGISTER_COMMAND, COMMAND_WRITE_SECTORS_EXT);
    } else {
        uint8_t count = sectorCount == 256 ? 0 : sectorCount;
        outb(iobase + REGISTER_DEVICE, 0xE0 | (secondary << 4) |
                ((lba >> 24) & 0x0F));
        outb(iobase + REGISTER_SECTOR_COUNT, count);
        outb(iobase + REGISTER_LBA_LOW, lba & 0xFF);
        outb(iobase + REGISTER_LBA_MID, (lba >> 8) & 0xFF);
        outb(iobase + REGISTER_LBA_HIGH, (lba >> 16) & 0xFF);
        outb(iobase + REGISTER_COMMAND, COMMAND_WRITE_SECTORS);
    }

    for (size_t i = 0; i < sectorCount; i++) {
        uint8_t status = inb(iobase + REGISTER_STATUS);
        while (status & STATUS_BUSY) {
            status = inb(iobase + REGISTER_STATUS);
        }

        while (!(status & (STATUS_DATA_REQUEST | STATUS_ERROR |
                STATUS_DEVICE_FAULT))) {
            status = inb(iobase + REGISTER_STATUS);
        }

        if (status & (STATUS_ERROR | STATUS_DEVICE_FAULT)) return false;

        for (size_t j = 0; j < sectorSize / 2; j++) {
            uint16_t word = (unsigned char) buffer[2 * j] |
                    ((unsigned char) buffer[2 * j + 1] << 8);
            outw(iobase + REGISTER_DATA, word);
        }

        buffer += sectorSize;
    }

    uint8_t status = inb(iobase + REGISTER_STATUS);
    while (status & STATUS_BUSY) {
        status = inb(iobase + REGISTER_STATUS);
    }
    if (status & (STATUS_ERROR | STATUS_DEVICE_FAULT)) return false;

    return true;
}

AtaDevice::AtaDevice(AtaChannel* channel, bool secondary, uint64_t sectors,
        uint64_t sectorSize, bool lba48Supported) : Vnode(S_IFBLK | 0644,
        devFS->stats.st_dev) {
    this->channel = channel;
    this->secondary = secondary;
    this->sectors = sectors;
    this->sectorSize = sectorSize;
    this->lba48Supported = lba48Supported;

    stats.st_size = sectors * sectorSize;
    stats.st_blksize = sectorSize;
    stats.st_rdev = (uintptr_t) this;
    tempBuffer = xnew char[sectorSize];
}

AtaDevice::~AtaDevice() {
    delete tempBuffer;
}

bool AtaDevice::isSeekable() {
    return true;
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

ssize_t AtaDevice::pread(void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    if (size == 0) return 0;

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    AutoLock lock(&mutex);

    if (offset >= stats.st_size) return 0;

    char* buf = (char*) buffer;
    uint64_t sectorMask = sectorSize - 1;
    size_t bytesRead = 0;
    if (offset & sectorMask) {
        uint64_t unaligned = offset & sectorMask;

        uint64_t lba = offset / sectorSize;
        if (!channel->readSectors(tempBuffer, 1, lba, secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        size_t copySize = sectorSize - unaligned;
        if (copySize > size) copySize = size;
        memcpy(buf, tempBuffer + unaligned, copySize);

        bytesRead += copySize;
        size -= copySize;
        offset += copySize;
    }

    while (size >= sectorSize) {
        if (offset >= stats.st_size) return bytesRead;

        size_t sectorsRemaining = size / sectorSize;
        if (sectorsRemaining > 65536) {
            sectorsRemaining = 65536;
        }
        if (!lba48Supported && sectorsRemaining > 256) {
            sectorsRemaining = 256;
        }

        uint64_t lba = offset / sectorSize;
        if (!channel->readSectors(buf + bytesRead, sectorsRemaining, lba,
                secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        bytesRead += sectorsRemaining * sectorSize;
        size -= sectorsRemaining * sectorSize;
        offset += sectorsRemaining * sectorSize;
    }

    if (offset >= stats.st_size) return bytesRead;

    if (size > 0) {
        uint64_t lba = offset / sectorSize;
        if (!channel->readSectors(tempBuffer, 1, lba, secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        memcpy(buf + bytesRead, tempBuffer, size);
        bytesRead += size;
    }

    return bytesRead;
}

ssize_t AtaDevice::pwrite(const void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    if (size == 0) return 0;

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    AutoLock lock(&mutex);

    if (offset >= stats.st_size) {
        errno = ENOSPC;
        return 0;
    }

    char* buf = (char*) buffer;
    uint64_t sectorMask = sectorSize - 1;
    size_t bytesWritten = 0;
    if (offset & sectorMask) {
        uint64_t unaligned = offset & sectorMask;

        uint64_t lba = offset / sectorSize;
        if (!channel->readSectors(tempBuffer, 1, lba, secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        size_t copySize = sectorSize - unaligned;
        if (copySize > size) copySize = size;
        memcpy(tempBuffer + unaligned, buf, copySize);

        if (!channel->writeSectors(tempBuffer, 1, lba, secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        bytesWritten += copySize;
        size -= copySize;
        offset += copySize;
    }

    while (size >= sectorSize) {
        if (offset >= stats.st_size) goto finish;

        size_t sectorsRemaining = size / sectorSize;
        if (sectorsRemaining > 65536) {
            sectorsRemaining = 65536;
        }
        if (!lba48Supported && sectorsRemaining > 256) {
            sectorsRemaining = 256;
        }

        uint64_t lba = offset / sectorSize;
        if (!channel->writeSectors(buf + bytesWritten, sectorsRemaining, lba,
                secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        bytesWritten += sectorsRemaining * sectorSize;
        size -= sectorsRemaining * sectorSize;
        offset += sectorsRemaining * sectorSize;
    }

    if (offset >= stats.st_size) goto finish;

    if (size > 0) {
        uint64_t lba = offset / sectorSize;
        if (!channel->readSectors(tempBuffer, 1, lba, secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        memcpy(tempBuffer, buf + bytesWritten, size);

        if (!channel->writeSectors(tempBuffer, 1, lba, secondary, sectorSize)) {
            errno = EIO;
            return -1;
        }

        bytesWritten += size;
    }

finish:
    // Make sure to flush the cache immediately because we currently do not have
    // a way to shutdown the system.
    if (!channel->flushCache(secondary)) {
        errno = EIO;
        return -1;
    }

    return bytesWritten;
}
