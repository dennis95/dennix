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

/* gui/rect.h
 * Rectangle.
 */

#ifndef RECT_H
#define RECT_H

#include <stdbool.h>

struct Rectangle {
    int x;
    int y;
    int width;
    int height;
};

static inline bool isInRect(int x, int y, struct Rectangle rect) {
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
            y < rect.y + rect.height;
}

static inline bool rectEqual(struct Rectangle rect1, struct Rectangle rect2) {
    return rect1.x == rect2.x && rect1.y == rect2.y &&
            rect1.width == rect2.width && rect1.height == rect2.height;
}

#endif
