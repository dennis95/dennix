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

/* kernel/include/dennix/display.h
 * Display.
 */

#ifndef _DENNIX_DISPLAY_H
#define _DENNIX_DISPLAY_H

#include <stddef.h>
#include <dennix/devctl.h>

/* Set the display mode. The new mode is returned in info. */
#define DISPLAY_SET_MODE _DEVCTL(_IOCTL_INT, 0)
/* Get the display resolution. */
#define DISPLAY_GET_RESOLUTION _DEVCTL(_IOCTL_PTR, 3)
/* Draw to the screen. Only supported in LFB mode. */
#define DISPLAY_DRAW _DEVCTL(_IOCTL_PTR, 4)
/* Get the current video mode. */
#define DISPLAY_GET_VIDEO_MODE _DEVCTL(_IOCTL_PTR, 5)
/* Set the video mode. */
#define DISPLAY_SET_VIDEO_MODE _DEVCTL(_IOCTL_PTR, 6)
/* Make the current process the display owner. */
#define DISPLAY_ACQUIRE _DEVCTL(_IOCTL_VOID, 1)
/* Stop owning the display. */
#define DISPLAY_RELEASE _DEVCTL(_IOCTL_VOID, 2)

#define DISPLAY_MODE_QUERY 0
#define DISPLAY_MODE_TEXT 1
#define DISPLAY_MODE_LFB 2

struct display_resolution {
    unsigned int width;
    unsigned int height;
};

struct display_draw {
    void* lfb;
    size_t lfb_pitch;
    unsigned int lfb_x;
    unsigned int lfb_y;
    unsigned int draw_x;
    unsigned int draw_y;
    unsigned int draw_width;
    unsigned int draw_height;
};

struct video_mode {
    unsigned int video_height;
    unsigned int video_width;
    unsigned int video_bpp; /* 0 for text mode */
};

#undef RGB
#undef RGBA
/* Supported alpha values are 0 (transparent) and 255 (not transparent). */
#define RGBA(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define RGB(r, g, b) RGBA(r, g, b, 255U)

#endif
