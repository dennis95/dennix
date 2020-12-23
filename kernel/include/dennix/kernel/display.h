/* Copyright (c) 2018, 2019, 2020 Dennis Wölfing
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

/* kernel/include/dennix/kernel/display.h
 * Display class.
 */

#ifndef KERNEL_DISPLAY_H
#define KERNEL_DISPLAY_H

#include <dennix/display.h>
#include <dennix/kernel/vnode.h>

struct CharPos {
    unsigned int x;
    unsigned int y;

    bool operator==(const CharPos& p) { return p.x == x && p.y == y; }
};

struct Color {
    uint32_t fgColor;
    uint32_t bgColor;
    uint8_t vgaColor;

    bool operator!=(const Color& other) {
        return fgColor != other.fgColor || bgColor != other.bgColor ||
                vgaColor != other.vgaColor;
    }
};

struct CharBufferEntry {
    wchar_t wc;
    Color color;
    bool modified;

    bool operator!=(const CharBufferEntry& other) {
        return wc != other.wc || color != other.color;
    }
};

class Display : public Vnode {
public:
    Display(video_mode mode, char* buffer, size_t pitch);
    void clear(CharPos from, CharPos to, Color color);
    int devctl(int command, void* restrict data, size_t size,
            int* restrict info) override;
    void initialize();
    void putCharacter(CharPos position, wchar_t c, Color color);
    void scroll(unsigned int lines, Color color, bool up = true);
    void setCursorPos(CharPos position);
    void setCursorVisibility(bool visible);
    void switchBuffer(Color color);
    void update();
private:
    char* charAddress(CharPos position);
    void redraw(CharPos position);
    void redraw(CharPos position, CharBufferEntry* entry);
    void setPixelColor(char* addr, uint32_t rgbColor);
    int setVideoMode(video_mode* videoMode);
public:
    unsigned int columns;
    unsigned int rows;
private:
    char* buffer;
    video_mode mode;
    size_t pitch;
    CharPos cursorPos;
    bool cursorVisible;
    CharBufferEntry* doubleBuffer;
    CharBufferEntry* primaryBuffer;
    CharBufferEntry* alternateBuffer;
    bool invalidated;
    bool renderingText;
    bool haveOldBuffer;
};

class GraphicsDriver {
public:
    virtual bool isSupportedMode(video_mode mode) = 0;
    virtual vaddr_t setVideoMode(video_mode* mode) = 0;
};

extern GraphicsDriver* graphicsDriver;

#endif
