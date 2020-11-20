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

/* kernel/include/dennix/kernel/mouse.h
 * Mouse device.
 */

#ifndef KERNEL_MOUSE_H
#define KERNEL_MOUSE_H

#include <dennix/mouse.h>
#include <dennix/kernel/vnode.h>

class MouseDevice : public Vnode {
public:
    MouseDevice();
    void addPacket(mouse_data data);
    virtual short poll();
    virtual ssize_t read(void* buffer, size_t size);
private:
    mouse_data mouseBuffer[256];
    size_t readIndex;
    size_t available;
};

extern Reference<MouseDevice> mouseDevice;

#endif
