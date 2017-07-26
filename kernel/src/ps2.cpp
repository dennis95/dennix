/* Copyright (c) 2016, 2017 Dennis Wölfing
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

/* kernel/src/ps2.cpp
 * PS/2 Controller.
 */

#include <inttypes.h>
#include <dennix/kernel/interrupts.h>
#include <dennix/kernel/log.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/ps2.h>
#include <dennix/kernel/ps2keyboard.h>
#include <dennix/kernel/terminal.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

#define COMMAND_READ_CONFIG 0x20
#define COMMAND_WRITE_CONFIG 0x60
#define COMMAND_DISABLE_PORT2 0xA7
#define COMMAND_ENABLE_PORT2 0xA8
#define COMMAND_TEST_PORT2 0xA9
#define COMMAND_SELF_TEST 0xAA
#define COMMAND_TEST_PORT1 0xAB
#define COMMAND_DISABLE_PORT1 0xAD
#define COMMAND_ENABLE_PORT1 0xAE

#define DEVICE_IDENTIFY 0xF2
#define DEVICE_DISABLE_SCANNING 0xF5
#define DEVICE_RESET 0xFF

static void checkPort1();
static void irqHandler(int irq);
static uint8_t readDataPort();
static void sendPS2Command(uint8_t command);
static void sendPS2Command(uint8_t command, uint8_t data);
static uint8_t sendPS2CommandWithResponse(uint8_t command);

static PS2Device* ps2Device1;
//static PS2Device* ps2Device2;

void PS2::initialize() {
    // Disable PS/2 devices
    sendPS2Command(COMMAND_DISABLE_PORT1);
    sendPS2Command(COMMAND_DISABLE_PORT2);

    // Flush the output buffer
    while (inb(PS2_STATUS_PORT) & 1) {
        inb(PS2_DATA_PORT);
    }

    // Configure the controller
    uint8_t config = sendPS2CommandWithResponse(COMMAND_READ_CONFIG);
    config &= ~((1 << 0) | (1 << 1));
    sendPS2Command(COMMAND_WRITE_CONFIG, config);

    uint8_t test = sendPS2CommandWithResponse(COMMAND_SELF_TEST);
    if (test != 0x55) {
        Log::printf("PS/2 self test failed (response = 0x%" PRIX8 ")\n", test);
        return;
    }

    bool dualChannel = false;

    if (config & (1 << 5)) {
        sendPS2Command(COMMAND_ENABLE_PORT2);
        if (sendPS2CommandWithResponse(COMMAND_READ_CONFIG) & (1 << 5)) {
            dualChannel = true;
        }
    }

    bool port1Exists = sendPS2CommandWithResponse(COMMAND_TEST_PORT1) == 0x00;
    bool port2Exists = dualChannel &&
            sendPS2CommandWithResponse(COMMAND_TEST_PORT2) == 0x00;

    if (!port1Exists && !port2Exists) {
        Log::printf("No usable PS/2 port found\n");
    }

    if (port1Exists) {
        sendPS2Command(COMMAND_ENABLE_PORT1);
    }

    if (port2Exists) {
        sendPS2Command(COMMAND_ENABLE_PORT2);
    }

    config = sendPS2CommandWithResponse(COMMAND_READ_CONFIG);
    if (port1Exists) {
        config |= (1 << 0);
    }

    if (port2Exists) {
        config |= (1 << 1);
    }

    sendPS2Command(COMMAND_WRITE_CONFIG, config);

    // Scan for connected devices
    if (port1Exists) {
        checkPort1();
    }

    if (port2Exists) {
        //TODO: Check PS/2 port 2
    }
}

static void checkPort1() {
#ifdef BROKEN_PS2_EMULATION
    /* On some computers PS/2 emulation is completely broken. In this case we
       just assume that there is a keyboard connected to port 1 that just
       works without any additional initialization. */
    PS2Keyboard* keyboard = new PS2Keyboard();
    keyboard->listener = (Terminal*) terminal;

    ps2Device1 = keyboard;
    Interrupts::irqHandlers[1] = irqHandler;
#else
    if (PS2::sendDeviceCommand(DEVICE_RESET) != 0xFA) return;
    if (readDataPort() != 0xAA) return;

    if (PS2::sendDeviceCommand(DEVICE_DISABLE_SCANNING) != 0xFA) return;

    if (PS2::sendDeviceCommand(DEVICE_IDENTIFY) != 0xFA) return;
    if (readDataPort() == 0xAB) {
        uint8_t id = readDataPort();
        if (id == 0x41 || id == 0xC1 || id == 0x83) {
            // The device identified itself as a keyboard
            PS2Keyboard* keyboard = new PS2Keyboard();
            keyboard->listener = (Terminal*) terminal;

            ps2Device1 = keyboard;
            Interrupts::irqHandlers[1] = irqHandler;
        }
    }
#endif
}

uint8_t PS2::sendDeviceCommand(uint8_t command) {
    uint8_t response;
    for (int i = 0; i < 3; i++) {
        while (inb(PS2_STATUS_PORT) & 2);
        outb(PS2_DATA_PORT, command);
        response = readDataPort();
        if (response != 0xFE) return response;
    }
    return response;
}

uint8_t PS2::sendDeviceCommand(uint8_t command, uint8_t data) {
    uint8_t response;
    for (int i = 0; i < 3; i++) {
        while (inb(PS2_STATUS_PORT) & 2);
        outb(PS2_DATA_PORT, command);
        while (inb(PS2_STATUS_PORT) & 2);
        outb(PS2_DATA_PORT, data);
        response = readDataPort();
        if (response != 0xFE) return response;
    }
    return response;
}

static void irqHandler(int irq) {
    if (irq == 1 && ps2Device1) {
        ps2Device1->irqHandler();
    }
}

static uint8_t readDataPort() {
    while (!(inb(PS2_STATUS_PORT) & 1));
    return inb(PS2_DATA_PORT);
}

static void sendPS2Command(uint8_t command) {
    while (inb(PS2_STATUS_PORT) & 2);
    outb(PS2_COMMAND_PORT, command);
}

static void sendPS2Command(uint8_t command, uint8_t data) {
    while (inb(PS2_STATUS_PORT) & 2);
    outb(PS2_COMMAND_PORT, command);
    while (inb(PS2_STATUS_PORT) & 2);
    outb(PS2_DATA_PORT, data);
}

static uint8_t sendPS2CommandWithResponse(uint8_t command) {
    while (inb(PS2_STATUS_PORT) & 2);
    outb(PS2_COMMAND_PORT, command);
    return readDataPort();
}
