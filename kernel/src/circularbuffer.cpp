/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/src/circularbuffer.cpp
 * Circular Buffer.
 */

#include <string.h>
#include <dennix/kernel/circularbuffer.h>

CircularBuffer::CircularBuffer() {

}

CircularBuffer::CircularBuffer(char* buffer, size_t size) {
    initialize(buffer, size);
}

void CircularBuffer::initialize(char* buffer, size_t size) {
    this->buffer = buffer;
    bufferSize = size;
    readPosition = 0;
    bytesStored = 0;
}

size_t CircularBuffer::bytesAvailable() {
    return bytesStored;
}

size_t CircularBuffer::spaceAvailable() {
    return bufferSize - bytesStored;
}

size_t CircularBuffer::read(void* buf, size_t size) {
    size_t bytesRead = 0;
    while (bytesStored > 0 && bytesRead < size) {
        size_t count = bufferSize - readPosition;
        if (count > size - bytesRead) count = size - bytesRead;
        if (count > bytesStored) count = bytesStored;

        memcpy((char*) buf + bytesRead, buffer + readPosition, count);
        readPosition = (readPosition + count) % bufferSize;
        bytesStored -= count;
        bytesRead += count;
    }
    return bytesRead;
}

size_t CircularBuffer::write(const void* buf, size_t size) {
    size_t written = 0;
    while (spaceAvailable() > 0 && written < size) {
        size_t writeIndex = (readPosition + bytesStored) % bufferSize;
        size_t count = bufferSize - writeIndex;
        if (count > size - written) count = size - written;
        if (count > spaceAvailable()) count = spaceAvailable();

        memcpy(buffer + writeIndex, (char*) buf + written, count);
        written += count;
        bytesStored += count;
    }
    return written;
}
