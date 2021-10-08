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

/* libdxui/src/context.c
 * The dxui context.
 */

#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dennix/display.h>
#include <dennix/mouse.h>
#include "context.h"

static dxui_context* initializeWithCompositor(int flags,
        const char* socketPath);
static dxui_context* initializeStandalone(int flags);

static bool readAll(const char* filename, void* buffer, size_t size) {
    char* buf = buffer;

    int fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    size_t bytes = 0;
    while (bytes < size) {
        ssize_t bytesRead = read(fd, buf + bytes, size - bytes);
        if (bytesRead < 0 && errno == EINTR) {
            // Try again.
        } else if (bytesRead <= 0) {
            close(fd);
            return false;
        } else {
            bytes += bytesRead;
        }
    }
    close(fd);
    return true;
}

dxui_dim dxui_get_display_dim(dxui_context* context) {
    return context->displayDim;
}

dxui_context* dxui_initialize(int flags) {
    dxui_context* context;
    const char* socketPath = getenv("DENNIX_GUI_SOCKET");
    if (socketPath) {
        context = initializeWithCompositor(flags, socketPath);
    } else {
        context = initializeStandalone(flags);
    }
    if (!context) return NULL;

    if (!readAll("/share/fonts/vgafont", &context->vgafont,
            sizeof(context->vgafont))) {
        dxui_shutdown(context);
        return NULL;
    }
    return context;
}

static dxui_context* initializeWithCompositor(int flags,
        const char* socketPath) {
    (void) flags;

    struct sockaddr_un addr;
    if (strlen(socketPath) >= sizeof(addr.sun_path)) return NULL;

    dxui_context* context = calloc(1, sizeof(dxui_context));
    if (!context) return NULL;
    context->backend = &dxui_compositorBackend;

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath);

    context->socket = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (context->socket < 0) {
        free(context);
        return NULL;
    }

    if (connect(context->socket, (struct sockaddr*) &addr, sizeof(addr)) < 0 &&
            errno != EINTR) {
        close(context->socket);
        free(context);
        return NULL;
    }

    if (!dxui_pump_events(context, DXUI_PUMP_ONCE, -1) ||
            context->displayDim.width < 0 || context->displayDim.height < 0) {
        close(context->socket);
        free(context);
        return NULL;
    }

    return context;
}

static dxui_context* initializeStandalone(int flags) {
    if (flags & DXUI_INIT_NEED_COMPOSITOR) return NULL;

    dxui_context* context = calloc(1, sizeof(dxui_context));
    if (!context) return NULL;
    context->socket = -1;
    context->backend = &dxui_standaloneBackend;
    context->mouseFd = -1;
    context->consoleFd = -1;

    context->displayFd = open("/dev/display", O_RDONLY | O_CLOEXEC);
    if (context->displayFd < 0) {
        free(context);
        return NULL;
    }

    int mode = DISPLAY_MODE_QUERY;
    int oldMode;
    if (posix_devctl(context->displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            &oldMode) != 0 || oldMode != DISPLAY_MODE_TEXT) {
        close(context->displayFd);
        free(context);
        return NULL;
    }

    mode = DISPLAY_MODE_LFB;
    if (posix_devctl(context->displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
            NULL) != 0) {
        dxui_shutdown(context);
        return NULL;
    }

    struct display_resolution res;
    if (posix_devctl(context->displayFd, DISPLAY_GET_RESOLUTION, &res,
            sizeof(res), NULL) != 0) {
        dxui_shutdown(context);
        return NULL;
    }

    context->displayDim.width = res.width;
    context->displayDim.height = res.height;

    context->framebuffer = malloc(res.width * res.height * sizeof(dxui_color));
    if (!context->framebuffer) {
        dxui_shutdown(context);
        return NULL;
    }

    context->mouseFd = open("/dev/mouse", O_RDONLY | O_CLOEXEC);
    if (context->mouseFd < 0) {
        dxui_shutdown(context);
        return NULL;
    }

    // Discard any buffered mouse movements.
    struct pollfd pfd[1];
    pfd[0].fd = context->mouseFd;
    pfd[0].events = POLLIN;
    while (poll(pfd, 1, 0) == 1) {
        struct mouse_data data[256];
        read(context->mouseFd, data, sizeof(data));
    }

    context->consoleFd = open("/dev/console", O_RDONLY | O_CLOEXEC);
    if (context->consoleFd < 0) {
        dxui_shutdown(context);
        return NULL;
    }

    struct termios termios;
    if (tcgetattr(context->consoleFd, &termios) < 0) {
        dxui_shutdown(context);
        return NULL;
    }

    termios.c_lflag |= _KBWC;
    if (tcsetattr(context->consoleFd, TCSAFLUSH, &termios) < 0) {
        dxui_shutdown(context);
        return NULL;
    }

    if (flags & DXUI_INIT_CURSOR) {
        size_t cursorSize = 48 * 48 * sizeof(dxui_color);
        context->cursors = malloc(5 * cursorSize);
        if (!readAll("/share/cursors/arrow.rgba", context->cursors,
                cursorSize)) {
            dxui_shutdown(context);
            return NULL;
        }
        if (!readAll("/share/cursors/resize_diagonal1.rgba",
                (char*) context->cursors + cursorSize, cursorSize)) {
            memcpy((char*) context->cursors + cursorSize, context->cursors,
                    cursorSize);
        }
        if (!readAll("/share/cursors/resize_diagonal2.rgba",
                (char*) context->cursors + 2 * cursorSize, cursorSize)) {
            memcpy((char*) context->cursors + 2 * cursorSize, context->cursors,
                    cursorSize);
        }
        if (!readAll("/share/cursors/resize_horizontal.rgba",
                (char*) context->cursors + 3 * cursorSize, cursorSize)) {
            memcpy((char*) context->cursors + 3 * cursorSize, context->cursors,
                    cursorSize);
        }
        if (!readAll("/share/cursors/resize_vertical.rgba",
                (char*) context->cursors + 4 * cursorSize, cursorSize)) {
            memcpy((char*) context->cursors + 4 * cursorSize, context->cursors,
                    cursorSize);
        }
    }

    context->mousePos.x = res.width / 2;
    context->mousePos.y = res.height / 2;

    return context;
}

bool dxui_is_standalone(dxui_context* context) {
    return context->socket == -1;
}

void dxui_shutdown(dxui_context* context) {
    if (!context) return;

    if (context->socket != -1) {
        close(context->socket);
    } else {
        free(context->cursors);
        free(context->framebuffer);

        int mode = DISPLAY_MODE_TEXT;
        posix_devctl(context->displayFd, DISPLAY_SET_MODE, &mode, sizeof(mode),
                NULL);
        close(context->displayFd);
        close(context->mouseFd);

        if (context->consoleFd != -1) {
            struct termios termios;
            tcgetattr(context->consoleFd, &termios);
            termios.c_lflag &= ~_KBWC;
            tcsetattr(context->consoleFd, TCSAFLUSH, &termios);
            close(context->consoleFd);
        }
    }
    free(context);
}
