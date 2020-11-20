/* Copyright (c) 2020 Dennis Wölfing
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

/* kernel/src/ps2mouse.cpp
 * PS/2 mouse driver.
 */

#include <dennix/kernel/log.h>
#include <dennix/kernel/mouse.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/ps2mouse.h>

#define MOUSE_GET_ID 0xF2
#define MOUSE_SET_SAMPLE_RATE 0xF3

#define PS2_MOUSE_LEFT_BUTTON (1 << 0)
#define PS2_MOUSE_RIGHT_BUTTON (1 << 1)
#define PS2_MOUSE_MIDDLE_BUTTON (1 << 2)
#define PS2_MOUSE_X_NEGATIVE (1 << 4)
#define PS2_MOUSE_Y_NEGATIVE (1 << 5)
#define PS2_MOUSE_X_OVERFLOW (1 << 6)
#define PS2_MOUSE_Y_OVERFLOW (1 << 7)

PS2Mouse::PS2Mouse(bool secondPort) : secondPort(secondPort) {
    hasMouseWheel = false;
    PS2::sendDeviceCommand(secondPort, MOUSE_GET_ID);
    uint8_t id = PS2::readDataPort();
    if (id == 0x00) {
        // Execute the detection sequence for mice with a mouse wheel.
        PS2::sendDeviceCommand(secondPort, MOUSE_SET_SAMPLE_RATE, 200, true);
        PS2::sendDeviceCommand(secondPort, MOUSE_SET_SAMPLE_RATE, 100, true);
        PS2::sendDeviceCommand(secondPort, MOUSE_SET_SAMPLE_RATE, 80, true);
        PS2::sendDeviceCommand(secondPort, MOUSE_GET_ID);
        id = PS2::readDataPort();
        if (id == 0x03) {
            hasMouseWheel = true;
        }
    }
    PS2::sendDeviceCommand(secondPort, MOUSE_SET_SAMPLE_RATE, 40, true);
    mouseDevice = xnew MouseDevice();
    Log::printf("PS/2 mouse found\n");
}

void PS2Mouse::irqHandler() {
    buffer[index++] = inb(0x60);
    if (index == 1 && !(buffer[0] & (1 << 3))) {
        // This is an invalid first byte.
        index = 0;
    }

    if (index == 4 || (index == 3 && !hasMouseWheel)) {
        index = 0;

        if (buffer[0] & (PS2_MOUSE_X_OVERFLOW | PS2_MOUSE_Y_OVERFLOW)) {
            // Overflow, discard the packet.
        } else {
            mouse_data data;
            data.mouse_flags = 0;
            if (buffer[0] & PS2_MOUSE_LEFT_BUTTON) {
                data.mouse_flags |= MOUSE_LEFT;
            }
            if (buffer[0] & PS2_MOUSE_RIGHT_BUTTON) {
                data.mouse_flags |= MOUSE_RIGHT;
            }
            if (buffer[0] & PS2_MOUSE_MIDDLE_BUTTON) {
                data.mouse_flags |= MOUSE_MIDDLE;
            }

            if (hasMouseWheel && (buffer[3] & 0xF) == 0x1) {
                data.mouse_flags |= MOUSE_SCROLL_DOWN;
            } else if (hasMouseWheel && (buffer[3] & 0xF) == 0xF) {
                data.mouse_flags |= MOUSE_SCROLL_UP;
            }

            if (buffer[0] & PS2_MOUSE_X_NEGATIVE) {
                data.mouse_x = -0x100 + buffer[1];
            } else {
                data.mouse_x = buffer[1];
            }

            if (buffer[0] & PS2_MOUSE_Y_NEGATIVE) {
                data.mouse_y = 0x100 - buffer[2];
            } else {
                data.mouse_y = -buffer[2];
            }

            mouseDevice->addPacket(data);
        }
    }
}
