/* Copyright (c) 2020, 2021 Dennis Wölfing
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

/* kernel/include/dennix/kernel/ps2mouse.h
 * PS/2 mouse driver.
 */

#ifndef KERNEL_PS2MOUSE_H
#define KERNEL_PS2MOUSE_H

#include <dennix/kernel/mouse.h>
#include <dennix/kernel/ps2.h>
#include <dennix/kernel/worker.h>

class PS2Mouse : public PS2Device {
public:
    PS2Mouse(bool secondPort);
    void irqHandler() override;
private:
    void work();
    static void worker(void* self);
private:
    uint8_t buffer[4];
    bool hasMouseWheel;
    unsigned char index;
    bool secondPort;
    Reference<MouseDevice> mouseDevice;
    mouse_data packetBuffer[128];
    size_t packetsAvailable;
    WorkerJob job;
};

#endif
