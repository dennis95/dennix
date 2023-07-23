/* Copyright (c) 2021, 2023 Dennis Wölfing
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

/* kernel/include/dennix/kernel/ata.h
 * ATA driver.
 */

#include <dennix/kernel/blockcache.h>
#include <dennix/kernel/interrupts.h>

namespace AtaController {
    void initialize(uint8_t bus, uint8_t device, uint8_t function);
};

class AtaChannel {
public:
    AtaChannel(uint16_t iobase, uint16_t ctrlbase, uint16_t busmasterBase,
            unsigned int irq, paddr_t prdPhys, vaddr_t prdVirt);
    ~AtaChannel() = default;
    NOT_COPYABLE(AtaChannel);
    NOT_MOVABLE(AtaChannel);

    bool flushCache(bool secondary);
    void identifyDevice(bool secondary);
    void onIrq(const InterruptContext* context);
    bool readSectors(char* buffer, size_t sectorCount, uint64_t lba,
            bool secondary, uint64_t sectorSize);
    bool writeSectors(const char* buffer, size_t sectorCount, uint64_t lba,
            bool secondary, uint64_t sectorSize);
private:
    bool finishDmaTransfer();
    bool setSectors(size_t sectorCount, uint64_t lba, bool secondary);
private:
    kthread_mutex_t mutex;
    uint16_t iobase;
    uint16_t ctrlbase;
    uint16_t busmasterBase;
    paddr_t prdPhys;
    vaddr_t prdVirt;
    paddr_t dmaRegion;
    vaddr_t dmaMapped;
    IrqHandler irqHandler;
    bool awaitingInterrupt;
    bool dmaInProgress;
    bool error;
};

class AtaDevice : public BlockCacheDevice {
public:
    AtaDevice(AtaChannel* channel, bool secondary, uint64_t sectors,
            uint64_t sectorSize, bool lba48Supported);
    ~AtaDevice() = default;
    NOT_COPYABLE(AtaDevice);
    NOT_MOVABLE(AtaDevice);

    off_t lseek(off_t offset, int whence) override;
    short poll() override;
    int sync(int flags) override;
protected:
    bool readUncached(void* buffer, size_t size, off_t offset, int flags)
            override;
    bool writeUncached(const void* buffer, size_t size, off_t offset, int flags)
            override;
private:
    AtaChannel* channel;
    uint64_t sectors;
    uint64_t sectorSize;
    bool secondary;
    bool lba48Supported;
};
