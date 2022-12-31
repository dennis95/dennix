/* Copyright (c) 2020, 2021, 2022 Dennis Wölfing
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

/* kernel/src/mouse.cpp
 * Mouse device.
 */

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <dennix/poll.h>
#include <dennix/kernel/devices.h>
#include <dennix/kernel/mouse.h>
#include <dennix/kernel/signal.h>

#define BUFFER_ITEMS (sizeof(mouseBuffer) / sizeof(mouse_data))

Reference<MouseDevice> mouseDevice;
AbsoluteMouseDriver* absoluteMouseDriver = nullptr;

MouseDevice::MouseDevice() : Vnode(S_IFCHR | 0666, DevFS::dev) {
    readIndex = 0;
    available = 0;
    readCond = KTHREAD_COND_INITIALIZER;
}

void MouseDevice::addPacket(mouse_data data) {
    AutoLock lock(&mutex);

    if (available == BUFFER_ITEMS) {
        // If the buffer is full then probably noone is reading, so we will just
        // discard the oldest packet.
        available--;
        readIndex = (readIndex + 1) % BUFFER_ITEMS;
    }

    size_t writeIndex = (readIndex + available) % BUFFER_ITEMS;
    mouseBuffer[writeIndex] = data;
    available++;
    kthread_cond_broadcast(&readCond);
}

int MouseDevice::devctl(int command, void* restrict data, size_t size,
            int* restrict info) {
    AutoLock lock(&mutex);

    switch (command) {
    case MOUSE_SET_ABSOLUTE: {
        if (size != 0 && size != sizeof(int)) {
            *info = -1;
            return EINVAL;
        }

        int* enabled = (int*) data;
        if (absoluteMouseDriver) {
            absoluteMouseDriver->setAbsoluteMouse(*enabled);
        } else if (*enabled) {
            *info = -1;
            return ENOTSUP;
        }

        *info = 0;
        return 0;
    } break;
    default:
        *info = -1;
        return EINVAL;
    }
}

short MouseDevice::poll() {
    AutoLock lock(&mutex);
    if (available) return POLLIN | POLLRDNORM;
    return 0;
}

ssize_t MouseDevice::read(void* buffer, size_t size, int flags) {
    AutoLock lock(&mutex);
    // We only allow reads of whole packets to prevent synchronization issues.
    size_t packets = size / sizeof(mouse_data);
    mouse_data* buf = (mouse_data*) buffer;

    for (size_t i = 0; i < packets; i++) {
        while (!available) {
            if (i > 0) return i * sizeof(mouse_data);

            if (flags & O_NONBLOCK) {
                errno = EAGAIN;
                return -1;
            }

            if (kthread_cond_sigwait(&readCond, &mutex) == EINTR) {
                errno = EINTR;
                return -1;
            }
        }

        buf[i] = mouseBuffer[readIndex];
        readIndex = (readIndex + 1) % BUFFER_ITEMS;
        available--;
    }

    return packets * sizeof(mouse_data);
}
