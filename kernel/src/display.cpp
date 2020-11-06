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

/* kernel/src/display.cpp
 * Display class.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dennix/kernel/addressspace.h>
#include <dennix/kernel/display.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/portio.h>
#include <dennix/kernel/terminaldisplay.h>

// Classical VGA font but with the Unicode replacement character at 0xFF.
asm(".pushsection .rodata\n"
".global vgafont\n"
"vgafont:\n\t"
    ".incbin \"../vgafont\"\n"
".popsection");
extern const uint8_t vgafont[];
GraphicsDriver* graphicsDriver;

static const size_t charHeight = 16;
static const size_t charWidth = 9;

static uint8_t unicodeToCp437(wchar_t wc);

Display::Display(video_mode mode, char* buffer, size_t pitch)
        : Vnode(S_IFCHR | 0666, 0) {
    this->buffer = buffer;
    this->mode = mode;
    if (mode.video_bpp == 0) {
        rows = mode.video_height;
        columns = mode.video_width;
    } else {
        rows = mode.video_height / charHeight;
        columns = (mode.video_width + 1) / charWidth;
    }
    this->pitch = pitch;
    cursorPos = {0, 0};
    cursorVisible = true;
    doubleBuffer = nullptr;
    primaryBuffer = nullptr;
    alternateBuffer = nullptr;
    invalidated = false;
    renderingText = true;
    haveOldBuffer = true;
}

ALWAYS_INLINE char* Display::charAddress(CharPos position) {
    return buffer + position.y * charHeight * pitch +
            position.x * charWidth * mode.video_bpp / 8;
}

ALWAYS_INLINE void Display::setPixelColor(char* addr,
        uint32_t rgbColor) {
    if ((rgbColor & 0xFF000000) == 0) return;
    if (mode.video_bpp == 32) {
        uint32_t* pixel = (uint32_t*) addr;
        *pixel = rgbColor;
    } else {
        addr[0] = (rgbColor >> 0) & 0xFF;
        addr[1] = (rgbColor >> 8) & 0xFF;
        addr[2] = (rgbColor >> 16) & 0xFF;
    }
}

void Display::clear(CharPos from, CharPos to, Color color) {
    size_t bufferStart = from.x + columns * from.y;
    size_t bufferEnd = to.x + columns * to.y;
    for (size_t i = bufferStart; i <= bufferEnd; i++) {
        if (doubleBuffer[i].wc != L'\0' || doubleBuffer[i].color != color) {
            doubleBuffer[i].wc = L'\0';
            doubleBuffer[i].color = color;
            doubleBuffer[i].modified = true;
        }
    }
}

void Display::initialize() {
    primaryBuffer = (CharBufferEntry*) malloc(rows * columns *
            sizeof(CharBufferEntry));
    if (!primaryBuffer) PANIC("Allocation failure");
    alternateBuffer = (CharBufferEntry*) malloc(rows * columns *
            sizeof(CharBufferEntry));
    if (!alternateBuffer) PANIC("Allocation failure");
    doubleBuffer = primaryBuffer;
    Color defaultColor = { RGB(170, 170, 170), RGB(0, 0, 0), 0x07 };
    clear({0, 0}, {columns - 1, rows - 1}, defaultColor);
}

void Display::putCharacter(CharPos position, wchar_t wc, Color color) {
    if (unlikely(!doubleBuffer)) {
        CharBufferEntry entry = { wc, color.fgColor, color.bgColor, true };
        redraw(position, &entry);
        return;
    }

    doubleBuffer[position.x + columns * position.y].wc = wc;
    doubleBuffer[position.x + columns * position.y].color = color;
    doubleBuffer[position.x + columns * position.y].modified = true;
}

void Display::redraw(CharPos position) {
    CharBufferEntry* entry = &doubleBuffer[position.x + columns * position.y];
    redraw(position, entry);
}

void Display::redraw(CharPos position, CharBufferEntry* entry) {
    entry->modified = false;
    wchar_t wc = entry->wc;

    if (mode.video_bpp == 0) {
        uint8_t vgaColor = entry->color.vgaColor;
        uint8_t cp437 = unicodeToCp437(wc);
        if (cp437 == 0xFF) {
            // Print unrepresentable characters as ? with inverted colors.
            cp437 = '?';
            vgaColor = ((vgaColor & 0x0F) << 4) | ((vgaColor & 0xF0) >> 4);
        }
        char* addr = buffer + 2 * position.y * 80 + 2 * position.x;
        addr[0] = cp437;
        addr[1] = vgaColor;
        return;
    }

    uint32_t foreground = entry->color.fgColor;
    uint32_t background = entry->color.bgColor;
    uint8_t cp437 = unicodeToCp437(wc);
    const uint8_t* charFont = vgafont + cp437 * 16;
    char* addr = charAddress(position);

    for (size_t i = 0; i < 16; i++) {
        for (size_t j = 0; j < 8; j++) {
            bool pixelFg = charFont[i] & (1 << (7 - j)) ||
                    (cursorVisible && position == cursorPos && i >= 14);
            uint32_t rgbColor = pixelFg ? foreground : background;
            setPixelColor(&addr[j * mode.video_bpp / 8], rgbColor);
        }

        if (likely((position.x + 1) * charWidth <= mode.video_width)) {
            bool pixelFg = cp437 >= 0xB0 && cp437 <= 0xDF && charFont[i] & 1;
            uint32_t rgbColor = pixelFg ? foreground : background;
            setPixelColor(&addr[8 * mode.video_bpp / 8], rgbColor);
        }
        addr += pitch;
    }
}

void Display::scroll(unsigned int lines, Color color, bool up /*= true*/) {
    CharBufferEntry empty;
    empty.wc = L'\0';
    empty.color = color;

    if (up) {
        for (unsigned int y = 0; y < rows; y++) {
            for (unsigned int x = 0; x < columns; x++) {
                CharBufferEntry& entry = y < rows - lines ?
                        doubleBuffer[x + (y + lines) * columns] : empty;
                if (doubleBuffer[x + y * columns] != entry) {
                    doubleBuffer[x + y * columns].wc = entry.wc;
                    doubleBuffer[x + y * columns].color = entry.color;
                    doubleBuffer[x + y * columns].modified = true;
                }
            }
        }
    } else {
        for (unsigned int y = rows - 1; y < rows; y--) {
            for (unsigned int x = 0; x < columns; x++) {
                CharBufferEntry& entry = y >= lines ?
                        doubleBuffer[x + (y - lines) * columns] : empty;
                if (doubleBuffer[x + y * columns] != entry) {
                    doubleBuffer[x + y * columns].wc = entry.wc;
                    doubleBuffer[x + y * columns].color = entry.color;
                    doubleBuffer[x + y * columns].modified = true;
                }
            }
        }
    }
}

void Display::setCursorPos(CharPos position) {
    if (mode.video_bpp == 0) {
        uint16_t value = position.x + position.y * 80;
        outb(0x3D4, 0x0E);
        outb(0x3D5, (value >> 8) & 0xFF);
        outb(0x3D4, 0x0F);
        outb(0x3D5, value & 0xFF);
    } else {
        if (unlikely(!doubleBuffer)) return;
        CharPos oldPos = cursorPos;
        cursorPos = position;
        doubleBuffer[oldPos.x + oldPos.y * columns].modified = true;
        doubleBuffer[cursorPos.x + cursorPos.y * columns].modified = true;
    }
}

void Display::setCursorVisibility(bool visible) {
    cursorVisible = visible;
    if (mode.video_bpp == 0) {
        if (visible) {
            outb(0x3D4, 0x0A);
            outb(0x3D5, 14);
        } else {
            outb(0x3D4, 0x0A);
            outb(0x3D5, 0x20);
        }
    } else {
        if (unlikely(!doubleBuffer)) return;
        doubleBuffer[cursorPos.x + cursorPos.y * columns].modified = true;
    }
}

int Display::setVideoMode(video_mode* videoMode) {
    if (!graphicsDriver->isSupportedMode(*videoMode)) return ENOTSUP;

    size_t newRows = videoMode->video_height / charHeight;
    size_t newColumns = (videoMode->video_width + 1) / charWidth;
    size_t newSize = newRows * newColumns;
    size_t oldSize = rows * columns;

    if (newSize > oldSize) {
        bool primary = doubleBuffer == primaryBuffer;
        CharBufferEntry* newPrimaryBuffer = (CharBufferEntry*)
                reallocarray(primaryBuffer, newSize, sizeof(CharBufferEntry));
        if (!newPrimaryBuffer) return ENOMEM;
        primaryBuffer = newPrimaryBuffer;
        CharBufferEntry* newAlternateBuffer = (CharBufferEntry*)
                reallocarray(alternateBuffer, newSize, sizeof(CharBufferEntry));
        if (!newAlternateBuffer) return ENOMEM;
        alternateBuffer = newAlternateBuffer;
        doubleBuffer = primary ? primaryBuffer : alternateBuffer;
    }

    vaddr_t framebuffer = graphicsDriver->setVideoMode(videoMode);

    if (!framebuffer) return EIO;
    if (haveOldBuffer) {
        size_t oldBufferSize = ALIGNUP(mode.video_height * pitch, PAGESIZE);
        kernelSpace->unmapPhysical((vaddr_t) buffer, oldBufferSize);
        haveOldBuffer = false;
    }
    buffer = (char*) framebuffer;
    mode = *videoMode;
    pitch = mode.video_width * (mode.video_bpp / 8);

    size_t oldRows = rows;
    size_t oldColumns = columns;

    newRows = videoMode->video_height / charHeight;
    newColumns = (videoMode->video_width + 1) / charWidth;

    CharBufferEntry blank;
    blank.wc = L'\0';
    blank.color.bgColor = RGB(0, 0, 0);
    blank.color.fgColor = RGB(170, 170, 170);
    blank.color.vgaColor = 0x07;
    blank.modified = true;

    if (cursorPos.y >= newRows) {
        scroll(cursorPos.y - newRows + 1, blank.color);
    }
    rows = newRows;
    columns = newColumns;
    TerminalDisplay::updateDisplaySize();

    if (newColumns <= oldColumns) {
        for (size_t i = 0; i < newRows; i++) {
            if (i < oldRows) {
                memmove(&doubleBuffer[i * newColumns],
                        &doubleBuffer[i * oldColumns],
                        newColumns * sizeof(CharBufferEntry));
            } else {
                for (size_t j = 0; j < newColumns; j++) {
                    doubleBuffer[i * newColumns + j] = blank;
                }
            }
        }
    } else {
        for (size_t i = newRows; i > 0; i--) {
            if (i < oldRows) {
                memmove(&doubleBuffer[(i - 1) * newColumns],
                        &doubleBuffer[(i - 1) * oldColumns],
                        oldColumns * sizeof(CharBufferEntry));
                for (size_t j = oldColumns; j < newColumns; j++) {
                    doubleBuffer[(i - 1) * newColumns + j] = blank;
                }
            } else {
                for (size_t j = 0; j < newColumns; j++) {
                    doubleBuffer[(i - 1) * newColumns + j] = blank;
                }
            }
        }
    }

    if (newSize < oldSize) {
        CharBufferEntry* newDoubleBuffer = (CharBufferEntry*)
                reallocarray(doubleBuffer, newSize, sizeof(CharBufferEntry));
        if (newDoubleBuffer) {
            doubleBuffer = newDoubleBuffer;
        }
    }

    invalidated = true;
    return 0;
}

void Display::switchBuffer(Color color) {
    if (doubleBuffer == primaryBuffer) {
        doubleBuffer = alternateBuffer;
        clear({0, 0}, {columns - 1, rows - 1}, color);
    } else {
        doubleBuffer = primaryBuffer;
    }
    invalidated = true;
}

void Display::update() {
    if (!renderingText || !doubleBuffer) return;
    bool redrawAll = invalidated;
    invalidated = false;

    for (unsigned int y = 0; y < rows; y++) {
        for (unsigned int x = 0; x < columns; x++) {
            if (redrawAll || doubleBuffer[x + y * columns].modified) {
                redraw({x, y});
            }
        }
    }
}

int Display::devctl(int command, void* restrict data, size_t size,
        int* restrict info) {
    switch (command) {
    case DISPLAY_SET_MODE: {
        if (size != 0 && size != sizeof(int)) {
            *info = -1;
            return EINVAL;
        }

        const int* displayMode = (const int*) data;

        if (*displayMode == DISPLAY_MODE_QUERY) {
            *info = renderingText ? DISPLAY_MODE_TEXT : DISPLAY_MODE_LFB;
            return 0;
        } else if (*displayMode == DISPLAY_MODE_TEXT) {
            if (!renderingText) {
                invalidated = true;
            }
            renderingText = true;
            *info = DISPLAY_MODE_TEXT;
            return 0;
        } else if (*displayMode == DISPLAY_MODE_LFB &&
                this->mode.video_bpp != 0) {
            renderingText = false;
            *info = DISPLAY_MODE_LFB;
            return 0;
        } else {
            *info = renderingText ? DISPLAY_MODE_TEXT : DISPLAY_MODE_LFB;
            return ENOTSUP;
        }
    } break;
    case DISPLAY_GET_RESOLUTION: {
        if (size != 0 && size != sizeof(struct display_resolution)) {
            *info = -1;
            return EINVAL;
        }

        if (mode.video_bpp == 0) {
            *info = -1;
            return ENOTSUP;
        }

        struct display_resolution* res = (struct display_resolution*) data;
        res->width = mode.video_width;
        res->height = mode.video_height;
        *info = 0;
        return 0;
    } break;
    case DISPLAY_DRAW: {
        if (size != 0 && size != sizeof(struct display_draw)) {
            *info = -1;
            return EINVAL;
        }

        if (renderingText) {
            *info = -1;
            return ENOTSUP;
        }

        const struct display_draw* draw = (const struct display_draw*) data;
        for (size_t y = 0; y < draw->draw_height; y++) {
            const uint32_t* row = (const uint32_t*) ((uintptr_t) draw->lfb +
                    (draw->draw_y + y) * draw->lfb_pitch);
            for (size_t x = 0; x < draw->draw_width; x++) {
                char* addr = buffer + (y + draw->lfb_y + draw->draw_y) * pitch +
                        (x + draw->lfb_x + draw->draw_x) * mode.video_bpp / 8;
                setPixelColor(addr, row[draw->draw_x + x]);
            }
        }

        *info = 0;
        return 0;
    } break;
    case DISPLAY_GET_VIDEO_MODE: {
        if (size != 0 && size != sizeof(struct video_mode)) {
            *info = -1;
            return EINVAL;
        }

        video_mode* videoMode = (video_mode*) data;
        *videoMode = mode;
        *info = 0;
        return 0;
    }
    case DISPLAY_SET_VIDEO_MODE: {
        if (size != 0 && size != sizeof(struct video_mode)) {
            *info = -1;
            return EINVAL;
        }

        if (!graphicsDriver) {
            *info = -1;
            return ENOTSUP;
        }

        video_mode* videoMode = (video_mode*) data;

        int errnum = setVideoMode(videoMode);
        *info = errnum ? -1 : 0;
        return errnum;
    }
    default:
        *info = -1;
        return EINVAL;
    }
}

static uint8_t unicodeToCp437(wchar_t wc) {
    // Translate Unicode characters into the code page 437 character set.
    // Note that some cp437 characters are used to represent multiple different
    // characters.
    if (wc == 0 || (wc >= 0x20 && wc <= 0x7E)) return wc;

    switch (wc) {
    case L'☺': return 0x01;
    case L'☻': return 0x02;
    case L'♥': return 0x03;
    case L'♦': return 0x04;
    case L'♣': return 0x05;
    case L'♠': return 0x06;
    case L'•': return 0x07;
    case L'◘': return 0x08;
    case L'○': return 0x09;
    case L'◙': return 0x0A;
    case L'♂': return 0x0B;
    case L'♀': return 0x0C;
    case L'♪': return 0x0D;
    case L'♫': return 0x0E;
    case L'☼': return 0x0F;
    case L'►': return 0x10;
    case L'◄': return 0x11;
    case L'↕': return 0x12;
    case L'‼': return 0x13;
    case L'¶': return 0x14;
    case L'§': return 0x15;
    case L'▬': return 0x16;
    case L'↨': return 0x17;
    case L'↑': return 0x18;
    case L'↓': return 0x19;
    case L'→': return 0x1A;
    case L'←': return 0x1B;
    case L'∟': return 0x1C;
    case L'↔': return 0x1D;
    case L'▲': return 0x1E;
    case L'▼': return 0x1F;
    case L'Δ': return 0x7F; // U+0394 GREEK CAPITAL LETTER DELTA
    case L'⌂': return 0x7F; // U+2302 HOUSE
    case L'Ç': return 0x80;
    case L'ü': return 0x81;
    case L'é': return 0x82;
    case L'â': return 0x83;
    case L'ä': return 0x84;
    case L'à': return 0x85;
    case L'å': return 0x86;
    case L'ç': return 0x87;
    case L'ê': return 0x88;
    case L'ë': return 0x89;
    case L'è': return 0x8A;
    case L'ï': return 0x8B;
    case L'î': return 0x8C;
    case L'ì': return 0x8D;
    case L'Ä': return 0x8E;
    case L'Å': return 0x8F;
    case L'É': return 0x90;
    case L'æ': return 0x91;
    case L'Æ': return 0x92;
    case L'ô': return 0x93;
    case L'ö': return 0x94;
    case L'ò': return 0x95;
    case L'û': return 0x96;
    case L'ù': return 0x97;
    case L'ÿ': return 0x98;
    case L'Ö': return 0x99;
    case L'Ü': return 0x9A;
    case L'¢': return 0x9B;
    case L'£': return 0x9C;
    case L'¥': return 0x9D;
    case L'₧': return 0x9E;
    case L'ƒ': return 0x9F;
    case L'á': return 0xA0;
    case L'í': return 0xA1;
    case L'ó': return 0xA2;
    case L'ú': return 0xA3;
    case L'ñ': return 0xA4;
    case L'Ñ': return 0xA5;
    case L'ª': return 0xA6;
    case L'º': return 0xA7;
    case L'¿': return 0xA8;
    case L'⌐': return 0xA9;
    case L'¬': return 0xAA;
    case L'½': return 0xAB;
    case L'¼': return 0xAC;
    case L'¡': return 0xAD;
    case L'«': return 0xAE;
    case L'»': return 0xAF;
    case L'░': return 0xB0;
    case L'▒': return 0xB1;
    case L'▓': return 0xB2;
    case L'│': return 0xB3; // U+2502 BOX DRAWINGS LIGHT VERTICAL
    case L'▏': return 0xB3; // U+258F LEFT ONE EIGHTH BLOCK
    case L'┤': return 0xB4;
    case L'╡': return 0xB5;
    case L'╢': return 0xB6;
    case L'╖': return 0xB7;
    case L'╕': return 0xB8;
    case L'╣': return 0xB9;
    case L'║': return 0xBA;
    case L'╗': return 0xBB;
    case L'╝': return 0xBC;
    case L'╜': return 0xBD;
    case L'╛': return 0xBE;
    case L'┐': return 0xBF;
    case L'└': return 0xC0;
    case L'┴': return 0xC1;
    case L'┬': return 0xC2;
    case L'├': return 0xC3;
    case L'─': return 0xC4;
    case L'┼': return 0xC5;
    case L'╞': return 0xC6;
    case L'╟': return 0xC7;
    case L'╚': return 0xC8;
    case L'╔': return 0xC9;
    case L'╩': return 0xCA;
    case L'╦': return 0xCB;
    case L'╠': return 0xCC;
    case L'═': return 0xCD;
    case L'╬': return 0xCE;
    case L'╧': return 0xCF;
    case L'╨': return 0xD0;
    case L'╤': return 0xD1;
    case L'╥': return 0xD2;
    case L'╙': return 0xD3;
    case L'╘': return 0xD4;
    case L'╒': return 0xD5;
    case L'╓': return 0xD6;
    case L'╫': return 0xD7;
    case L'╪': return 0xD8;
    case L'┘': return 0xD9;
    case L'┌': return 0xDA;
    case L'█': return 0xDB;
    case L'▄': return 0xDC;
    case L'▌': return 0xDD;
    case L'▐': return 0xDE;
    case L'▀': return 0xDF;
    case L'α': return 0xE0;
    case L'ß': return 0xE1; // U+00DF LATIN SMALL LETTER SHARP S
    case L'β': return 0xE1; // U+03B2 GREEK SMALL LETTER BETA
    case L'Γ': return 0xE2;
    case L'π': return 0xE3;
    case L'Σ': return 0xE4; // U+03A3 GREEK CAPITAL LETTER SIGMA
    case L'∑': return 0xE4; // U+2211 N-ARY SUMMATION
    case L'σ': return 0xE5;
    case L'µ': return 0xE6; // U+00B5 MICRO SIGN
    case L'μ': return 0xE6; // U+03BC GREEK SMALL LETTER MU
    case L'τ': return 0xE7;
    case L'Φ': return 0xE8;
    case L'Θ': return 0xE9;
    case L'Ω': return 0xEA; // U+03A9 GREEK CAPITAL LETTER OMEGA
    case L'Ω': return 0xEA; // U+2126 OHM SIGN
    case L'ð': return 0xEB; // U+00F0 LATIN SMALL LETTER ETH
    case L'δ': return 0xEB; // U+03B4 GREEK SMALL LETTER DELTA
    case L'∂': return 0xEB; // U+2202 PARTIAL DIFFERENTIAL
    case L'∞': return 0xEC;
    case L'Ø': return 0xED; // U+00D8 LATIN CAPITAL LETTER O WITH STROKE
    case L'ø': return 0xED; // U+00F8 LATIN SMALL LETTER O WITH STROKE
    case L'φ': return 0xED; // U+03C6 GREEK SMALL LETTER PHI
    case L'ϕ': return 0xED; // U+03D5 GREEK PHI SYMBOL
    case L'∅': return 0xED; // U+2205 EMPTY SET
    case L'⌀': return 0xED; // U+2300 DIAMETER SIGN
    case L'𝜙': return 0xED; // U+1D719 MATHEMATICAL ITALIC PHI SYMBOL
    case L'ε': return 0xEE; // U+03B5 GREEK SMALL LETTER EPSILON
    case L'€': return 0xEE; // U+20AC EURO SIGN
    case L'∈': return 0xEE; // U+2208 ELEMENT OF
    case L'∩': return 0xEF;
    case L'≡': return 0xF0;
    case L'±': return 0xF1;
    case L'≥': return 0xF2;
    case L'≤': return 0xF3;
    case L'⌠': return 0xF4;
    case L'⌡': return 0xF5;
    case L'÷': return 0xF6;
    case L'≈': return 0xF7;
    case L'°': return 0xF8;
    case L'∙': return 0xF9;
    case L'·': return 0xFA;
    case L'√': return 0xFB;
    case L'ⁿ': return 0xFC;
    case L'²': return 0xFD;
    case L'■': return 0xFE;

    // Use 0xFF for characters that cannot be represented by cp437.
    default: return 0xFF;
    }
}
