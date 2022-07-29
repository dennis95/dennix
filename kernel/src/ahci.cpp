/* Copyright (c) 2022 Dennis Wölfing
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

/* kernel/src/ahci.cpp
 * Advanced Host Controller Interface driver.
 */

#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <dennix/poll.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/ahci.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/partition.h>
#include <dennix/kernel/pci.h>
#include <dennix/kernel/physicalmemory.h>

#define REGISTER_CAP 0x00 // HBA Capabilities
#define REGISTER_GHC 0x04 // Global Host Control
#define REGISTER_IS 0x08 // Interrupt Status Register
#define REGISTER_PI 0x0C // Ports implemented
#define REGISTER_CAP2 0x24 // Host Capabilities Extended
#define REGISTER_BOHC 0x28 // BIOS/OS Handoff Control and Status

#define REGISTER_PxCLB 0x00 // Port Command List Base Address
#define REGISTER_PxCLBU 0x04 // Port Command List Base Address Upper
#define REGISTER_PxFB 0x08 // Port FIS Base Address
#define REGISTER_PxFBU 0x0C // Port FIS Base Address Upper
#define REGISTER_PxIS 0x10 // Port Interrupt Status
#define REGISTER_PxIE 0x14 // Port Interrupt Enable
#define REGISTER_PxCMD 0x18 // Port Command and Status
#define REGISTER_PxTFD 0x20 // Port Task File Data
#define REGISTER_PxSIG 0x24 // Port Signature
#define REGISTER_PxSSTS 0x28 // Port Serial ATA Status
#define REGISTER_PxSERR 0x30 // Port Serial ATA Error
#define REGISTER_PxCI 0x38 // Port Command Issue

#define GHC_IE (1 << 1) // Interrupt Enable
#define GHC_AE (1U << 31) // AHCI Enable

#define CAP_S64A (1U << 31) // Supports 64-bit addressing

#define CAP2_BOH (1 << 0) // BIOS/OS Handoff

#define BOHC_BOS (1 << 0) // BIOS Owned Semaphore
#define BOHC_OOS (1 << 1) // OS Owned Semaphore
#define BOHC_BB (1 << 4) // BIOS Busy

#define PxCMD_ST (1 << 0) // Start
#define PxCMD_FRE (1 << 4) // FIS Receive Enable
#define PxCMD_FR (1 << 14) // FIS Receive Running
#define PxCMD_CR (1 << 15) // Command List Running

#define PxTFD_DRQ (1 << 3) // Device Request
#define PxTFD_BSY (1 << 7) // Busy

#define PxSIG_ATA 0x101

#define PxIE_DHRE (1 << 0) // Port Device to Host Register FIS Interrupt
#define PxIE_PSE (1 << 1) // Port PIO Setup FIS Interrupt
#define PxIE_DSE (1 << 2) // Port DMA Setup FIS Interrupt
#define PxIE_SDBE (1 << 3) // Port Set Device Bits FIS Interrupt
#define PxIE_DPE (1 << 5) // Port Descripor Processed Interrupt
#define PORT_INTERRUPT_ERROR 0x7DC00050

#define FIS_TYPE_REG_H2D 0x27

#define COMMAND_READ_DMA_EXT 0x25
#define COMMAND_WRITE_DMA_EXT 0x35
#define COMMAND_FLUSH_CACHE 0xE7
#define COMMAND_IDENTIFY_DEVICE 0xEC

static size_t numAhciDevices = 0;
static void onAhciIrq(void* user, const InterruptContext* context);

class AhciController {
public:
    AhciController(vaddr_t hbaMapped, unsigned int irq);
    void onIrq(const InterruptContext* context);
private:
    uint32_t readRegister(size_t offset);
    void writeRegister(size_t offset, uint32_t value);
private:
    vaddr_t hbaMapped;
    Reference<AhciDevice> ports[32];
    IrqHandler irqHandler;
};

void Ahci::initialize(uint8_t bus, uint8_t device, uint8_t function) {
    int irq = Pci::getIrq(bus, device, function);
    if (irq < 0) {
        Log::printf("AHCI controller unsupported: cannot use IRQs\n");
        return;
    }

    uint32_t bar5 = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, bar5));

    vaddr_t hbaMapped = kernelSpace->mapPhysical(bar5, 2 * PAGESIZE,
            PROT_READ | PROT_WRITE);
    if (!hbaMapped) PANIC("Failed to map AHCI registers");

    uint32_t cap = *(volatile uint32_t*) (hbaMapped + REGISTER_CAP);
    if (!(cap & CAP_S64A)) {
        kernelSpace->unmapPhysical(hbaMapped, 2 * PAGESIZE);
        Log::printf("AHCI controller unsupported: controller does not support "
                "64 bits\n");
        return;
    }

    xnew AhciController(hbaMapped, irq);
}

AhciController::AhciController(vaddr_t hbaMapped, unsigned int irq)
        : hbaMapped(hbaMapped), ports() {
    if (readRegister(REGISTER_CAP2) & CAP2_BOH) {
        // Perform BIOS OS handoff.
        uint32_t bohc = readRegister(REGISTER_BOHC);
        bohc |= BOHC_OOS;
        writeRegister(REGISTER_BOHC, bohc);

        while (bohc & (BOHC_BOS | BOHC_BB)) {
            bohc = readRegister(REGISTER_BOHC);
        }
    }

    // Switch to AHCI mode and disable interrupts.
    uint32_t ghc = readRegister(REGISTER_GHC);
    ghc |= GHC_AE;
    ghc &= ~GHC_IE;
    writeRegister(REGISTER_GHC, ghc);

    uint32_t pi = readRegister(REGISTER_PI);
    for (size_t i = 0; i < 32; i++) {
        if (!(pi & (1U << i))) continue;

        size_t portOffset = 0x100 + i * 0x80;

        // Switch the port to idle state.
        uint32_t cmd = readRegister(portOffset + REGISTER_PxCMD);
        cmd &= ~PxCMD_ST;
        writeRegister(portOffset + REGISTER_PxCMD, cmd);
        while (cmd & PxCMD_CR) {
            cmd = readRegister(portOffset + REGISTER_PxCMD);
        }
        cmd &= ~PxCMD_FRE;
        writeRegister(portOffset + REGISTER_PxCMD, cmd);
        while (cmd & PxCMD_FR) {
            cmd = readRegister(portOffset + REGISTER_PxCMD);
        }

        // Allocate memory for command lists and received FIS.
        paddr_t portMemPhys = PhysicalMemory::popPageFrame();
        if (!portMemPhys) PANIC("Failed to allocate memory for AHCI port");

        uint64_t commandList = portMemPhys;
        uint64_t receivedFis = portMemPhys + 0x400;

        vaddr_t portMemVirt = kernelSpace->mapPhysical(portMemPhys, PAGESIZE,
                PROT_READ | PROT_WRITE);
        if (!portMemVirt) PANIC("Failed to map memory for AHCI port");
        memset((void*) portMemVirt, 0, PAGESIZE);

        writeRegister(portOffset + REGISTER_PxCLB, commandList & 0xFFFFFFFF);
        writeRegister(portOffset + REGISTER_PxCLBU, commandList >> 32);
        writeRegister(portOffset + REGISTER_PxFB, receivedFis & 0xFFFFFFFF);
        writeRegister(portOffset + REGISTER_PxFBU, receivedFis >> 32);

        // Enable receiving FIS from the device.
        cmd |= PxCMD_FRE;
        writeRegister(portOffset + REGISTER_PxCMD, cmd);

        // Clear errors.
        uint32_t serr = readRegister(portOffset + REGISTER_PxSERR);
        writeRegister(portOffset + REGISTER_PxSERR, serr);

        // Clear interrupts.
        writeRegister(portOffset + REGISTER_PxIE, 0);
        writeRegister(portOffset + REGISTER_PxIS,
                readRegister(portOffset + REGISTER_PxIS));

        // Detect whether a device is connected to the port.
        uint32_t tfd = readRegister(portOffset + REGISTER_PxTFD);
        if (!(tfd & (PxTFD_DRQ | PxTFD_BSY))) {
            uint32_t ssts = readRegister(portOffset + REGISTER_PxSSTS);
            if ((ssts & 0x0F) == 0x03) {
                uint32_t sig = readRegister(portOffset + REGISTER_PxSIG);
                if (sig == PxSIG_ATA) {
                    // An ATA device was detected. Try to initialize it.
                    ports[i] = xnew AhciDevice(hbaMapped + portOffset,
                            portMemPhys, portMemVirt);
                }
            }
        }
    }

    irqHandler.func = onAhciIrq;
    irqHandler.user = this;
    Interrupts::addIrqHandler(irq, &irqHandler);

    // Enable AHCI interrupts.
    ghc |= GHC_IE;
    writeRegister(REGISTER_GHC, ghc);

    for (size_t i = 0; i < 32; i++) {
        if (!ports[i]) continue;

        if (!ports[i]->identify()) {
            Interrupts::disable();
            ports[i] = nullptr;
            Interrupts::enable();
        }
    }
}

static void onAhciIrq(void* user, const InterruptContext* context) {
    AhciController* device = (AhciController*) user;
    device->onIrq(context);
}

void AhciController::onIrq(const InterruptContext* context) {
    uint32_t interruptStatus = readRegister(REGISTER_IS);

    for (size_t i = 0; i < 32; i++) {
        if (interruptStatus & (1U << i) && ports[i]) {
            size_t portOffset = 0x100 + i * 0x80;
            uint32_t pxis = readRegister(portOffset + REGISTER_PxIS);
            writeRegister(portOffset + REGISTER_PxIS, pxis);
            writeRegister(REGISTER_IS, 1U << i);
            ports[i]->onIrq(context, pxis);
        }
    }
}

uint32_t AhciController::readRegister(size_t offset) {
    volatile uint32_t* reg = (volatile uint32_t*) (hbaMapped + offset);
    return *reg;
}

void AhciController::writeRegister(size_t offset, uint32_t value) {
    volatile uint32_t* reg = (volatile uint32_t*) (hbaMapped + offset);
    *reg = value;
}

AhciDevice::AhciDevice(vaddr_t portRegisters, paddr_t portMemPhys,
        vaddr_t portMemVirt) : BlockCacheDevice(0644, DevFS::dev) {
    this->portRegisters = portRegisters;
    this->portMemPhys = portMemPhys;
    this->portMemVirt = portMemVirt;

    awaitingInterrupt = false;
    dmaInProgress = false;
    error = 0;
}

bool AhciDevice::finishDmaTransfer() {
    if (!dmaInProgress) return true;

    while (awaitingInterrupt) {
        sched_yield();
    }

    dmaInProgress = false;
    if (error) {
        Log::printf("AHCI error 0x%X\n", error);
        error = 0;
        return false;
    }
    return true;
}

bool AhciDevice::identify() {
    // Start the DMA engine.
    uint32_t cmd = readRegister(REGISTER_PxCMD);
    cmd |= PxCMD_ST;
    writeRegister(REGISTER_PxCMD, cmd);

    // Enable interrupts.
    uint32_t ie = PxIE_DHRE | PxIE_PSE | PxIE_DSE | PxIE_SDBE | PxIE_DPE |
            PORT_INTERRUPT_ERROR;
    writeRegister(REGISTER_PxIE, ie);

    vaddr_t virt = kernelSpace->mapMemory(PAGESIZE, PROT_READ);
    if (!virt) return false;
    paddr_t phys = kernelSpace->getPhysicalAddress(virt);

    // Ask the device to identify itself.
    if (!sendDmaCommand(COMMAND_IDENTIFY_DEVICE, phys, 512, false, 0, 0) ||
            !finishDmaTransfer()) {
        kernelSpace->unmapMemory(virt, PAGESIZE);
        return false;
    }

    uint16_t* data = (uint16_t*) virt;

    if (data[0] & (1 << 15)) {
        kernelSpace->unmapMemory(virt, PAGESIZE);
        return false;
    }
    bool lba48Supported = data[83] & (1 << 10);

    if (lba48Supported) {
        sectors = data[100] | (data[101] << 16) | ((uint64_t) data[102] << 32) |
                ((uint64_t) data[103] << 48);
    } else {
        Log::printf("unsupported AHCI device: no lba48\n");
        kernelSpace->unmapMemory(virt, PAGESIZE);
        return false;
    }

    stats.st_blksize = 512;
    if ((data[106] & (1 << 14)) && !(data[106] & (1 << 15))) {
        if (data[106] & (1 << 12)) {
            stats.st_blksize = 2 * (data[117] | (data[118] << 16));
        }
    }
    kernelSpace->unmapMemory(virt, PAGESIZE);

    if (__builtin_mul_overflow(sectors, stats.st_blksize, &stats.st_size)) {
        return false;
    }

    char name[32];
    snprintf(name, sizeof(name), "ahci%zu", numAhciDevices++);
    devFS.addDevice(name, this);

    Partition::scanPartitions(this, name, stats.st_blksize);
    return true;
}

off_t AhciDevice::lseek(off_t offset, int whence) {
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

void AhciDevice::onIrq(const InterruptContext* /*context*/,
        uint32_t interruptStatus) {
    if (interruptStatus & PORT_INTERRUPT_ERROR) {
        error = interruptStatus & PORT_INTERRUPT_ERROR;
    }

    uint32_t commandIssue = readRegister(REGISTER_PxCI);
    if (!commandIssue) {
        awaitingInterrupt = false;
    }
}

short AhciDevice::poll() {
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

bool AhciDevice::readUncached(void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    assert(offset % stats.st_blksize == 0);
    assert(size % stats.st_blksize == 0);
    assert(size <= PAGESIZE);
    assert(offset < stats.st_size);

    size_t sectors = size / stats.st_blksize;
    uint64_t lba = offset / stats.st_blksize;

    vaddr_t virt = (vaddr_t) buffer;
    vaddr_t aligned = virt & ~PAGE_MISALIGN;
    paddr_t phys = kernelSpace->getPhysicalAddress(aligned);
    phys += virt - aligned;
    if (!sendDmaCommand(COMMAND_READ_DMA_EXT, phys, size, false,
            lba, sectors)) {
        return false;
    }
    if (!finishDmaTransfer()) return false;

    return true;
}

int AhciDevice::sync(int /*flags*/) {
    if (!sendDmaCommand(COMMAND_FLUSH_CACHE, 0, 0, false, 0, 0) ||
            !finishDmaTransfer()) {
        errno = EIO;
        return -1;
    }
    return 0;
}

bool AhciDevice::writeUncached(const void* buffer, size_t size, off_t offset,
        int /*flags*/) {
    assert(offset % stats.st_blksize == 0);
    assert(size % stats.st_blksize == 0);
    assert(size <= PAGESIZE);
    assert(offset < stats.st_size);

    size_t sectors = size / stats.st_blksize;
    uint64_t lba = offset / stats.st_blksize;

    vaddr_t virt = (vaddr_t) buffer;
    vaddr_t aligned = virt & ~PAGE_MISALIGN;
    paddr_t phys = kernelSpace->getPhysicalAddress(aligned);
    phys += virt - aligned;
    if (!sendDmaCommand(COMMAND_WRITE_DMA_EXT, phys, size, true, lba,
            sectors)) {
        errno = EIO;
        return false;
    }

    return true;
}

struct CommandHeader {
    uint16_t flags;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
};

struct CommandFis {
    uint8_t type;
    uint8_t flags;
    uint8_t command;
    uint8_t featuresLow;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featuresHigh;
    uint16_t count;
    uint8_t icc;
    uint8_t control;
    uint32_t auxiliary;
};

struct PrdtEntry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t byteCount;
};

struct CommandTable {
    CommandFis cfis;
    char padding[44];
    char acmd[16];
    char reserved[48];
    PrdtEntry entry;
};

bool AhciDevice::sendDmaCommand(uint8_t command, paddr_t physicalAddress,
        size_t size, bool write, uint64_t lba, uint16_t blockCount) {
    if (!finishDmaTransfer()) return false;

    CommandHeader* header = (CommandHeader*) portMemVirt;
    CommandTable* table = (CommandTable*) (portMemVirt + 0x500);

    CommandFis* cfis = &table->cfis;
    cfis->type = FIS_TYPE_REG_H2D;
    cfis->flags = 0x80;
    cfis->command = command;
    cfis->lba0 = lba & 0xFF;
    cfis->lba1 = (lba >> 8) & 0xFF;
    cfis->lba2 = (lba >> 16) & 0xFF;
    cfis->lba3 = (lba >> 24) & 0xFF;
    cfis->lba4 = (lba >> 32) & 0xFF;
    cfis->lba5 = (lba >> 40) & 0xFF;
    cfis->count = blockCount;
    cfis->device = 0x40;

    if (size > 0) {
        PrdtEntry* prdt = &table->entry;
        prdt->dba = physicalAddress & 0xFFFFFFFF;
        prdt->dbau = (uint64_t) physicalAddress >> 32;
        prdt->reserved = 0;
        prdt->byteCount = size - 1;
    }

    header->flags = 5;
    if (write) {
        header->flags |= (1 << 6);
    }
    header->prdtl = size ? 1 : 0;
    header->prdbc = 0;
    uint64_t tablePhys = portMemPhys + 0x500;
    header->ctba = tablePhys & 0xFFFFFFFF;
    header->ctbau = tablePhys >> 32;

    awaitingInterrupt = true;
    dmaInProgress = true;
    error = false;

    writeRegister(REGISTER_PxCI, 1);
    return true;
}

uint32_t AhciDevice::readRegister(size_t offset) {
    volatile uint32_t* reg = (volatile uint32_t*) (portRegisters + offset);
    return *reg;
}

void AhciDevice::writeRegister(size_t offset, uint32_t value) {
    volatile uint32_t* reg = (volatile uint32_t*) (portRegisters + offset);
    *reg = value;
}
