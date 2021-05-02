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

MouseDevice::MouseDevice() : Vnode(S_IFCHR | 0666,
        devFS.getRootDir()->stat().st_dev) {
    readIndex = 0;
    available = 0;
}

void MouseDevice::addPacket(mouse_data data) {
    if (available == BUFFER_ITEMS) {
        // If the buffer is full then probably noone is reading, so we will just
        // discard the oldest packet.
        available--;
        readIndex = (readIndex + 1) % BUFFER_ITEMS;
    }

    size_t writeIndex = (readIndex + available) % BUFFER_ITEMS;
    mouseBuffer[writeIndex] = data;
    available++;
}

short MouseDevice::poll() {
    if (available) return POLLIN | POLLRDNORM;
    return 0;
}

// TODO: Reading from the mouse device should be atomic.

ssize_t MouseDevice::read(void* buffer, size_t size, int flags) {
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

            if (Signal::isPending()) {
                errno = EINTR;
                return -1;
            }

            sched_yield();
        }

        buf[i] = mouseBuffer[readIndex];
        readIndex = (readIndex + 1) % BUFFER_ITEMS;
        available--;
    }

    return packets * sizeof(mouse_data);
}
