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

/* kernel/src/filedescription.cpp
 * FileDescription class.
 */

#include <dennix/kernel/filedescription.h>

FileDescription::FileDescription(Vnode* vnode) {
    this->vnode = vnode;
    offset = 0;
}

FileDescription* FileDescription::openat(const char* path, int /*flags*/,
        mode_t /*mode*/) {
    Vnode* node = resolvePath(vnode, path);
    if (!node) return nullptr;
    return new FileDescription(node);
}

ssize_t FileDescription::read(void* buffer, size_t size) {
    if (vnode->isSeekable()) {
        ssize_t result = vnode->pread(buffer, size, offset);

        if (result != -1) {
            offset += result;
        }
        return result;
    }
    return vnode->read(buffer, size);
}

ssize_t FileDescription::readdir(unsigned long offset, void* buffer,
        size_t size) {
    return vnode->readdir(offset, buffer, size);
}

int FileDescription::tcgetattr(struct termios* result) {
    return vnode->tcgetattr(result);
}

int FileDescription::tcsetattr(int flags, const struct termios* termio) {
    return vnode->tcsetattr(flags, termio);
}

ssize_t FileDescription::write(const void* buffer, size_t size) {
    if (vnode->isSeekable()) {
        ssize_t result = vnode->pwrite(buffer, size, offset);

        if (result != -1) {
            offset += result;
        }
        return result;
    }
    return vnode->write(buffer, size);
}
