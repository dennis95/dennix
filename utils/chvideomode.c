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

/* utils/chvideomode.c
 * Change video mode.
 */

#include <devctl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dennix/display.h>

static bool parseMode(const char* str, struct video_mode* mode);

int main(int argc, char* argv[]) {
    if (argc > 2) {
        errx(1, "extra operand '%s'", argv[2]);
    }
    if (argc == 2) {
        struct video_mode mode;
        if (!parseMode(argv[1], &mode)) {
            errx(1, "invalid videomode '%s'", argv[1]);
        }
        int fd = open("/dev/display", O_RDONLY);
        if (fd < 0) err(1, "open: '/dev/display'");
        struct video_mode modeCopy = mode;
        errno = posix_devctl(fd, DISPLAY_SET_VIDEO_MODE, &mode, sizeof(mode),
                NULL);
        if (errno) err(1, "cannot set video mode '%s'", argv[1]);
        close(fd);

        if (modeCopy.video_width != mode.video_width ||
                modeCopy.video_height != mode.video_height) {
            warnx("video mode was set to %ux%ux%u", mode.video_width,
                    mode.video_height, mode.video_bpp);
        }
    } else {
        struct video_mode mode;
        int fd = open("/dev/display", O_RDONLY);
        if (fd < 0) err(1, "open: '/dev/display'");
        errno = posix_devctl(fd, DISPLAY_GET_VIDEO_MODE, &mode, sizeof(mode),
                NULL);
        if (errno) err(1, "cannot get video mode");
        close(fd);
        printf("%ux%ux%u\n", mode.video_width, mode.video_height,
                mode.video_bpp);
    }
}

static bool parseMode(const char* str, struct video_mode* mode) {
    char* end;
    unsigned long width = strtoul(str, &end, 10);
    if (width == 0 || width > UINT_MAX) return false;
    if (*end != 'x') return false;
    unsigned long height = strtoul(end + 1, &end, 10);
    if (height == 0 || height > UINT_MAX) return false;
    unsigned long bpp = 32;
    if (*end == 'x') {
        bpp = strtoul(end + 1, &end, 10);
        if (bpp > 32) return false;
    }
    if (*end != '\0') return false;

    mode->video_width = width;
    mode->video_height = height;
    mode->video_bpp = bpp;
    return true;
}
