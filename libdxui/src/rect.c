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

/* libdxui/src/rect.c
 * Rectangle functions.
 */

#include <dxui.h>

bool dxui_rect_contains_pos(dxui_rect rect, dxui_pos pos) {
    return rect.x <= pos.x && pos.x - rect.x <= rect.width &&
            rect.y <= pos.y && pos.y - rect.y <= rect.height;
}

dxui_rect dxui_rect_crop(dxui_rect rect, dxui_dim dim) {
    dxui_rect cropRect;
    cropRect.pos.x = 0;
    cropRect.pos.y = 0;
    cropRect.dim = dim;
    return dxui_rect_intersect(rect, cropRect);
}

bool dxui_rect_equals(dxui_rect a, dxui_rect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width &&
            a.height == b.height;
}

dxui_rect dxui_rect_intersect(dxui_rect a, dxui_rect b) {
    if (a.x < b.x) {
        a.width -= b.x - a.x;
        a.x = b.x;
    }
    if (a.y < b.y) {
        a.height -= b.y - a.y;
        a.y = b.y;
    }

    if (a.x + a.width > b.x + b.width) {
        a.width = b.x + b.width - a.x;
    }
    if (a.y + a.height > b.y + b.height) {
        a.height = b.y + b.height - a.y;
    }

    if (a.width < 0 || a.height < 0) {
        a.width = 0;
        a.height = 0;
    }
    return a;
}
