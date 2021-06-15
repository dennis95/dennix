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

/* kernel/include/dennix/kernel/ata.h
 * ATA driver.
 */

#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/vnode.h>

namespace AtaController {
    void initialize(uint8_t bus, uint8_t device, uint8_t function);
};

class AtaChannel {
public:
    AtaChannel(uint16_t iobase, uint16_t ctrlbase, uint16_t busmasterBase,
            unsigned int irq);
    bool flushCache(bool secondary);
    void identifyDevice(bool secondary);
    void onIrq(const InterruptContext* context);
    bool readSectors(char* buffer, size_t sectorCount, uint64_t lba,
            bool secondary, uint64_t sectorSize);
    bool writeSectors(const char* buffer, size_t sectorCount, uint64_t lba,
            bool secondary, uint64_t sectorSize);
private:
    kthread_mutex_t mutex;
    uint16_t iobase;
    uint16_t ctrlbase;
    uint16_t busmasterBase;
    IrqHandler irqHandler;
};

class AtaDevice : public Vnode {
public:
    AtaDevice(AtaChannel* channel, bool secondary, uint64_t sectors,
            uint64_t sectorSize, bool lba48Supported);
    ~AtaDevice();
    bool isSeekable() override;
    off_t lseek(off_t offset, int whence) override;
    short poll() override;
    ssize_t pread(void* buffer, size_t size, off_t offset, int flags) override;
    ssize_t pwrite(const void* buffer, size_t size, off_t offset, int flags)
            override;
    int sync(int flags) override;
private:
    AtaChannel* channel;
    char* tempBuffer;
    uint64_t sectors;
    uint64_t sectorSize;
    bool secondary;
    bool lba48Supported;
};
