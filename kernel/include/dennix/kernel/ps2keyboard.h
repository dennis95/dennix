/* Copyright (c) 2016, 2020, 2021, 2023 Dennis Wölfing
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

/* kernel/include/dennix/kernel/ps2keyboard.h
 * PS/2 Keyboard driver.
 */

#ifndef KERNEL_PS2KEYBOARD_H
#define KERNEL_PS2KEYBOARD_H

#include <dennix/kernel/keyboard.h>
#include <dennix/kernel/ps2.h>
#include <dennix/kernel/worker.h>

class PS2Keyboard : public PS2Device {
public:
    PS2Keyboard(bool secondPort);
    ~PS2Keyboard() = default;
    NOT_COPYABLE(PS2Keyboard);
    NOT_MOVABLE(PS2Keyboard);

    void irqHandler() override;
private:
    void handleKey(int keycode);
    void work();
    static void worker(void* self);
public:
    KeyboardListener* listener;
private:
    bool secondPort;
    int buffer[128];
    size_t available;
    WorkerJob job;
};

#endif
