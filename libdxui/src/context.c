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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "context.h"

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
    (void) flags;

    const char* socketPath = getenv("DENNIX_GUI_SOCKET");
    if (!socketPath) return NULL;

    struct sockaddr_un addr;
    if (strlen(socketPath) >= sizeof(addr.sun_path)) return NULL;

    dxui_context* context = calloc(1, sizeof(dxui_context));
    if (!context) return NULL;

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

    if (!dxui_pump_events(context, DXUI_PUMP_ONCE) ||
            context->displayDim.width < 0 || context->displayDim.height < 0) {
        close(context->socket);
        free(context);
        return NULL;
    }

    if (!readAll("/share/fonts/vgafont", &context->vgafont,
            sizeof(context->vgafont))) {
        close(context->socket);
        free(context);
        return NULL;
    }

    return context;
}

void dxui_shutdown(dxui_context* context) {
    if (!context) return;

    close(context->socket);
    free(context);
}
