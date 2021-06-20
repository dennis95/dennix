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

/* libdxui/src/text.c
 * Text drawing.
 */

#include <wchar.h>
#include "context.h"
#include "cp437.h"

static const int fontHeight = 16;
static const int fontWidth = 9;

dxui_rect dxui_get_text_rect(const char* text, dxui_rect rect, int flags) {
    mbstate_t ps = {0};
    const char* s = text;
    size_t charLength = mbsrtowcs(NULL, &s, 0, &ps);
    size_t pixelLength = charLength * fontWidth - 1;

    dxui_rect result;

    if (flags & DXUI_TEXT_CENTERED) {
        result.x = rect.x + rect.width / 2 - pixelLength / 2;
        result.y = rect.y + rect.height / 2 - fontHeight / 2;
    } else {
        result.pos = rect.pos;
    }

    result.width = pixelLength;
    result.height = fontHeight;
    return result;
}

void dxui_draw_text(dxui_context* context, dxui_color* framebuffer,
        const char* text, dxui_color color, dxui_rect rect, dxui_rect crop,
        size_t pitch, int flags) {
    rect = dxui_get_text_rect(text, rect, flags);
    dxui_draw_text_in_rect(context, framebuffer, text, color, rect.pos, crop,
            pitch);
}

void dxui_draw_text_in_rect(dxui_context* context, dxui_color* framebuffer,
        const char* text, dxui_color color, dxui_pos pos, dxui_rect rect,
        size_t pitch) {
    mbstate_t ps = {0};
    wchar_t wc;

    while (*text) {
        size_t status = mbrtowc(&wc, text, 1, &ps);
        if (status == (size_t) -1) {
            ps = (mbstate_t) {0};
            wc = L'�';
        } else if (status == (size_t) -2) {
            text++;
            continue;
        }

        dxui_draw_text_wc(context, framebuffer, wc, color, pos, rect, pitch);
        pos.x += fontWidth;
        text++;
    }
}

void dxui_draw_text_wc(dxui_context* context, dxui_color* framebuffer,
        wchar_t wc, dxui_color color, dxui_pos pos, dxui_rect crop,
        size_t pitch) {
    uint8_t cp437 = unicodeToCp437(wc);

    const char* font = &context->vgafont[cp437 * fontHeight];
    for (int i = 0; i < fontHeight; i++) {
        int y = pos.y + i;
        if (y < crop.y) continue;
        if (y >= crop.y + crop.height) break;
        for (int j = 0; j < 8; j++) {
            int x = pos.x + j;
            if (x < crop.x) continue;
            if (x >= crop.x + crop.width) break;
            bool pixelFg = font[i] & (1 << (7 - j));
            if (pixelFg) {
                framebuffer[y * pitch + x] = color;
            }
        }
    }
}
