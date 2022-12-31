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

/* kernel/src/virtualbox.cpp
 * VirtualBox Guest Additions.
 */

#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/console.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/mouse.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/pci.h>
#include <dennix/kernel/physicalmemory.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/virtualbox.h>
#include <dennix/kernel/worker.h>

#define VBOX_VMMDEV_VERSION 0x10003
#define VBOX_REQUEST_HEADER_VERSION 0x10001

#define VBOX_REQUEST_GET_MOUSE 1
#define VBOX_REQUEST_SET_MOUSE 2
#define VBOX_REQUEST_ACK_EVENTS 41
#define VBOX_REQUEST_GUEST_INFO 50
#define VBOX_REQUEST_GET_DISPLAY_CHANGE 51
#define VBOX_REQUEST_SET_GUEST_CAPS 55

#define VBOX_CAP_GRAPHICS (1 << 2)

#define VBOX_MOUSE_ABSOLUTE (1 << 0)
#define VBOX_MOUSE_NEW_PROTOCOL (1 << 4)

#define VBOX_EVENT_DISPLAY_CHANGE (1 << 2)
#define VBOX_EVENT_MOUSE_POS (1 << 9)

struct VboxHeader {
    uint32_t size;
    uint32_t version;
    uint32_t requestType;
    int32_t rc;
    uint32_t reserved1;
    uint32_t reserved2;
};

struct VboxGuestInfo {
    VboxHeader header;
    uint32_t version;
    uint32_t ostype;
};

struct VboxGuestCaps {
    VboxHeader header;
    uint32_t caps;
};

struct VboxAckEvents {
    VboxHeader header;
    uint32_t events;
};

struct VboxDisplayChange {
    VboxHeader header;
    uint32_t xres;
    uint32_t yres;
    uint32_t bpp;
    uint32_t eventack;
};

struct VboxMouse {
    VboxHeader header;
    uint32_t mouseFeatures;
    int32_t x;
    int32_t y;
};

class VirtualBoxDevice : public AbsoluteMouseDriver {
public:
    VirtualBoxDevice(uint16_t port, volatile uint32_t* vmmdev, int irq);
    void onIrq(const InterruptContext* /*context*/);
    void setAbsoluteMouse(bool enabled) override;
    void work();
private:
    uint16_t port;
    volatile uint32_t* vmmdev;
    paddr_t requestPhysical;
    vaddr_t requestVirtual;
    IrqHandler irqHandler;
    uint32_t pendingEvents;
    WorkerJob workerJob;
};

static void onVboxIrq(void* device, const InterruptContext* context) {
    VirtualBoxDevice* dev = (VirtualBoxDevice*) device;
    dev->onIrq(context);
}

static void vboxWork(void* device) {
    VirtualBoxDevice* dev = (VirtualBoxDevice*) device;
    dev->work();
}

void VirtualBox::initialize(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t port = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, bar0)) & 0xFFFC;

    uint32_t bar1 = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, bar1)) & 0xFFFFFFFC;
    volatile uint32_t* vmmdev = (volatile uint32_t*)
            kernelSpace->mapPhysical(bar1, PAGESIZE, PROT_READ | PROT_WRITE);
    if (!vmmdev) PANIC("Failed to map page");

    // The VirtualBox device does not implement proper PCI interrupt routing and
    // instead always triggers a hardcoded IRQ that can be determined by reading
    // the interrupt line config. That's why we don't use Pci:getIrq.
    int irq = Pci::readConfig(bus, device, function,
            offsetof(PciHeader, interruptLine)) & 0xFF;

    xnew VirtualBoxDevice(port, vmmdev, irq);
}

VirtualBoxDevice::VirtualBoxDevice(uint16_t port, volatile uint32_t* vmmdev,
        int irq) : port(port), vmmdev(vmmdev) {
    requestPhysical = PhysicalMemory::popPageFrame32();
    if (!requestPhysical) {
        PANIC("Failed to allocate memory for VirtualBox Guest Additions");
    }

    requestVirtual = kernelSpace->mapPhysical(requestPhysical, PAGESIZE,
            PROT_READ | PROT_WRITE);
    if (!requestVirtual) {
        PANIC("Failed to map memory for VirtualBox Guest Additions");
    }

    pendingEvents = 0;
    workerJob.func = vboxWork;
    workerJob.context = this;

    irqHandler.func = onVboxIrq;
    irqHandler.user = this;
    Interrupts::addIrqHandler(irq, &irqHandler);

    // Identify ourselves.
    volatile VboxGuestInfo* info = (volatile VboxGuestInfo*) requestVirtual;
    info->header.size = sizeof(VboxGuestInfo);
    info->header.version = VBOX_REQUEST_HEADER_VERSION;
    info->header.requestType = VBOX_REQUEST_GUEST_INFO;
    info->header.rc = 0;
    info->header.reserved1 = 0;
    info->header.reserved2 = 0;
    info->version = VBOX_VMMDEV_VERSION;
#ifdef __x86_64__
    info->ostype = 0x100; // Unknown x86_64 OS
#else
    info->ostype = 0; // Unknown OS
#endif
    outl(port, requestPhysical);

    volatile VboxGuestCaps* caps = (volatile VboxGuestCaps*) requestVirtual;
    caps->header.size = sizeof(VboxGuestCaps);
    caps->header.version = VBOX_REQUEST_HEADER_VERSION;
    caps->header.requestType = VBOX_REQUEST_SET_GUEST_CAPS;
    caps->header.rc = 0;
    caps->header.reserved1 = 0;
    caps->header.reserved2 = 0;
    caps->caps = VBOX_CAP_GRAPHICS;
    outl(port, requestPhysical);

    absoluteMouseDriver = this;

    // Enable interrupts.
    vmmdev[3] = VBOX_EVENT_DISPLAY_CHANGE | VBOX_EVENT_MOUSE_POS;
}

void VirtualBoxDevice::onIrq(const InterruptContext* /*context*/) {
    uint32_t events = vmmdev[2];
    if (!events) return;

    if (!pendingEvents) {
        WorkerThread::addJob(&workerJob);
    }

    pendingEvents |= events;
}

void VirtualBoxDevice::setAbsoluteMouse(bool enabled) {
    Interrupts::disable();
    volatile VboxMouse* mouse = (volatile VboxMouse*) requestVirtual;
    mouse->header.size = sizeof(VboxMouse);
    mouse->header.version = VBOX_REQUEST_HEADER_VERSION;
    mouse->header.requestType = VBOX_REQUEST_SET_MOUSE;
    mouse->header.rc = 0;
    mouse->header.reserved1 = 0;
    mouse->header.reserved2 = 0;
    mouse->mouseFeatures = enabled ?
            VBOX_MOUSE_ABSOLUTE | VBOX_MOUSE_NEW_PROTOCOL : 0;
    mouse->x = 0;
    mouse->y = 0;
    outl(port, requestPhysical);
    Interrupts::enable();
}

void VirtualBoxDevice::work() {
    Interrupts::disable();
    uint32_t events = pendingEvents;
    pendingEvents = 0;
    Interrupts::enable();

    volatile VboxAckEvents* ack = (volatile VboxAckEvents*) requestVirtual;
    ack->header.size = sizeof(VboxAckEvents);
    ack->header.version = VBOX_REQUEST_HEADER_VERSION;
    ack->header.requestType = VBOX_REQUEST_ACK_EVENTS;
    ack->header.rc = 0;
    ack->header.reserved1 = 0;
    ack->header.reserved2 = 0;
    ack->events = events;
    outl(port, requestPhysical);

    if (events & VBOX_EVENT_DISPLAY_CHANGE) {
        volatile VboxDisplayChange* display =
                (volatile VboxDisplayChange*) requestVirtual;
        display->header.size = sizeof(VboxDisplayChange);
        display->header.version = VBOX_REQUEST_HEADER_VERSION;
        display->header.requestType = VBOX_REQUEST_GET_DISPLAY_CHANGE;
        display->header.rc = 0;
        display->header.reserved1 = 0;
        display->header.reserved2 = 0;
        display->xres = 0;
        display->yres = 0;
        display->bpp = 0;
        display->eventack = 1;
        outl(port, requestPhysical);

        video_mode mode;
        mode.video_width = display->xres;
        mode.video_height = display->yres;
        mode.video_bpp = display->bpp;
        console->display->setVideoMode(&mode);
    }

    if (events & VBOX_EVENT_MOUSE_POS) {
        volatile VboxMouse* mouse = (volatile VboxMouse*) requestVirtual;
        mouse->header.size = sizeof(VboxMouse);
        mouse->header.version = VBOX_REQUEST_HEADER_VERSION;
        mouse->header.requestType = VBOX_REQUEST_GET_MOUSE;
        mouse->header.rc = 0;
        mouse->header.reserved1 = 0;
        mouse->header.reserved2 = 0;
        mouse->mouseFeatures = 0;
        mouse->x = 0;
        mouse->y = 0;
        outl(port, requestPhysical);

        video_mode mode = console->display->getVideoMode();

        mouse_data data;
        data.mouse_x = (mouse->x * mode.video_width) / 0xFFFF;
        data.mouse_y = (mouse->y * mode.video_height) / 0xFFFF;
        data.mouse_flags = MOUSE_ABSOLUTE | MOUSE_NO_BUTTON_INFO;

        mouseDevice->addPacket(data);
    }
}
