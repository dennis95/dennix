/* Copyright (c) 2022, 2023 Dennis Wölfing
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

/* kernel/include/dennix/kernel/ahci.h
 * Advanced Host Controller Interface driver.
 */

#ifndef KERNEL_AHCI_H
#define KERNEL_AHCI_H

#include <dennix/kernel/blockcache.h>
#include <dennix/kernel/interrupts.h>

namespace Ahci {
void initialize(uint8_t bus, uint8_t device, uint8_t function);
}

class AhciDevice : public BlockCacheDevice {
public:
    AhciDevice(vaddr_t portRegisters, paddr_t portMemPhys, vaddr_t portMemVirt);
    ~AhciDevice() = default;
    NOT_COPYABLE(AhciDevice);
    NOT_MOVABLE(AhciDevice);

    bool identify();
    off_t lseek(off_t offset, int whence) override;
    void onIrq(const InterruptContext* context, uint32_t interruptStatus);
    short poll() override;
    int sync(int flags) override;
protected:
    bool readUncached(void* buffer, size_t size, off_t offset, int flags)
            override;
    bool writeUncached(const void* buffer, size_t size, off_t offset, int flags)
            override;
private:
    bool finishDmaTransfer();
    uint32_t readRegister(size_t offset);
    bool sendDmaCommand(uint8_t command, paddr_t physicalAddress, size_t size,
            bool write, uint64_t lba, uint16_t blockCount);
    void writeRegister(size_t offset, uint32_t value);
private:
    vaddr_t portRegisters;
    paddr_t portMemPhys;
    vaddr_t portMemVirt;
    uint32_t error;
    uint64_t sectors;
    uint64_t sectorSize;
    bool awaitingInterrupt;
    bool dmaInProgress;
};

#endif
