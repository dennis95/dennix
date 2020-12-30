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

/* gui/gui-test.c
 * GUI test program.
 */

#include <ctype.h>
#include <err.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dennix/display.h>
#include "guimsg.h"

struct Window {
    struct Window* prev;
    struct Window* next;
    unsigned int id;
    uint32_t* lfb;
    int width;
    int height;
    bool idAssigned;
    bool resized;
    char textBox[17];
};

static int fd;
static size_t headerReceived;
static struct gui_msg_header headerBuffer;
static char* messageBuffer;
static size_t messageReceived;
static bool mouseDown;
static struct Window* firstWindow;
static struct Window* lastWindow;
static char vgafont[4096];

static struct Window* addWindow(void);
static void createWindow(int width, int height, const char* title);
static void drawText(struct Window* window, int x, int y, int width, int height,
        const char* text);
static struct Window* getWindow(unsigned int id);
static void handleCloseButton(struct gui_event_window_close_button* msg);
static void handleKey(struct gui_event_key* msg);
static void handleMessage(unsigned int type, void* msg);
static void handleMouse(struct gui_event_mouse* msg);
static void handleResize(struct gui_event_window_resized* msg);
static void handleStatus(struct gui_event_status* msg);
static void handleWindowCreated(struct gui_event_window_created* msg);
static void loadFromFile(const char* filename, void* buffer, size_t size);
static void receiveMessage(void);
static void receiveMessages(void);
static void redraw(struct Window* window);
static void redrawPart(struct Window* window, int x, int y, int width,
        int height);
static void removeWindow(struct Window* window);
static void showWindow(struct Window* window);
static void writeAll(int fd, const void* buffer, size_t size);

static struct Window* addWindow(void) {
    struct Window* window = malloc(sizeof(struct Window));
    if (!window) err(1, "malloc");

    window->prev = NULL;
    window->next = firstWindow;
    firstWindow = window;
    if (window->next) {
        window->next->prev = window;
    } else {
        lastWindow = window;
    }

    window->height = 350;
    window->width = 500;
    window->lfb = malloc(window->width * window->height * sizeof(uint32_t));
    if (!window->lfb) err(1, "malloc");
    window->idAssigned = false;
    window->resized = false;
    strcpy(window->textBox, "                ");

    createWindow(window->width, window->height, "Hello World");

    while (!window->idAssigned) {
        receiveMessage();
    }

    redraw(window);
    showWindow(window);
    return window;
}

static void createWindow(int width, int height, const char* title) {
    struct gui_msg_create_window msg;
    msg.x = -1;
    msg.y = -1;
    msg.width = width;
    msg.height = height;
    msg.flags = 0;
    struct gui_msg_header header;
    header.type = GUI_MSG_CREATE_WINDOW;
    header.length = sizeof(msg) + strlen(title);

    writeAll(fd, &header, sizeof(header));
    writeAll(fd, &msg, sizeof(msg));
    writeAll(fd, title, strlen(title));
}

static void drawText(struct Window* window, int x, int y, int width, int height,
        const char* text) {
    if (x + width > window->width) {
        width = window->width - x;
    }
    if (y + height > window->height) {
        height = window->height - y;
    }

    for (int yPos = y; yPos < y + height; yPos++) {
        for (int xPos = x; xPos < x + width; xPos++) {
            window->lfb[yPos * window->width + xPos] = RGB(255, 255, 255);
        }
    }

    size_t length = strlen(text);
    for (int i = 0; i < (int) length; i++) {
        unsigned char c = text[i];
        const char* font = &vgafont[c * 16];
        for (int j = 0; j < 16; j++) {
            if (j >= height) break;
            for (int k = 0; k < 8; k++) {
                if (i * 9 + k >= width) break;
                bool pixelFg = font[j] & (1 << (7 - k));
                if (pixelFg) {
                    size_t index = (y + j) * window->width + i * 9 + k + x;
                    window->lfb[index] = RGB(0, 0, 0);
                }
            }
        }
    }
}

static struct Window* getWindow(unsigned int id) {
    for (struct Window* window = firstWindow; window; window = window->next) {
        if (window->idAssigned && window->id == id) {
            return window;
        }
    }

    return NULL;
}

static void handleCloseButton(struct gui_event_window_close_button* msg) {
    struct Window* window = getWindow(msg->window_id);
    if (!window) return;
    removeWindow(window);

    struct gui_msg_close_window response;
    response.window_id = msg->window_id;
    struct gui_msg_header responseHeader;
    responseHeader.type = GUI_MSG_CLOSE_WINDOW;
    responseHeader.length = sizeof(response);

    writeAll(fd, &responseHeader, sizeof(responseHeader));
    writeAll(fd, &response, sizeof(response));

    if (!firstWindow) exit(0);
}

static void handleKey(struct gui_event_key* msg) {
    struct Window* window = getWindow(msg->window_id);
    if (!window) return;
    if (!msg->codepoint) return;
    if (msg->codepoint > 0xFF || !isprint(msg->codepoint)) return;

    memmove(window->textBox, window->textBox + 1, 15);
    window->textBox[15] = msg->codepoint;
    drawText(window, 50, 150, 150, 30, window->textBox);
    redrawPart(window, 50, 150, 150, 30);
}

static void handleMessage(unsigned int type, void* msg) {
    switch (type) {
    case GUI_EVENT_CLOSE_BUTTON:
        handleCloseButton(msg);
        break;
    case GUI_EVENT_KEY:
        handleKey(msg);
        break;
    case GUI_EVENT_MOUSE:
        handleMouse(msg);
        break;
    case GUI_EVENT_STATUS:
        handleStatus(msg);
        break;
    case GUI_EVENT_WINDOW_CREATED:
        handleWindowCreated(msg);
        break;
    case GUI_EVENT_WINDOW_RESIZED:
        handleResize(msg);
        break;
    default:
        warnx("unknown message type %u\n", type);
    }
}

static void handleMouse(struct gui_event_mouse* msg) {
    bool click = !mouseDown && msg->flags & GUI_MOUSE_LEFT;
    mouseDown = msg->flags & GUI_MOUSE_LEFT;

    if (click && msg->x > 50 && msg->x < 200 && msg->y > 50 && msg->y < 80) {
        addWindow();
    }

    if (click && msg->x > 50 && msg->x < 200 && msg->y > 100 && msg->y < 130) {
        pid_t pid = fork();
        if (pid == 0) {
            execl("/bin/gui-test", "gui-test", NULL);
            _Exit(1);
        }
    }
}

static void handleResize(struct gui_event_window_resized* msg) {
    struct Window* window = getWindow(msg->window_id);
    if (!window) return;
    window->height = msg->height;
    window->width = msg->width;
    window->resized = true;
}

static void handleStatus(struct gui_event_status* msg) {
    // This message allows us to get the display resolution, but we don't need
    // this yet.
    (void) msg;
}

static void handleWindowCreated(struct gui_event_window_created* msg) {
    for (struct Window* window = firstWindow; window; window = window->next) {
        if (!window->idAssigned) {
            window->id = msg->window_id;
            window->idAssigned = true;
            return;
        }
    }
}

static void loadFromFile(const char* filename, void* buffer, size_t size) {
    FILE* file = fopen(filename, "r");
    if (!file) err(1, "cannot open '%s'", filename);
    size_t bytesRead = fread(buffer, 1, size, file);
    if (bytesRead < size) errx(1, "cannot load '%s", filename);
    fclose(file);
}

static void receiveMessage(void) {
    while (headerReceived < sizeof(struct gui_msg_header)) {
        ssize_t bytesRead = read(fd, (char*) &headerBuffer + headerReceived,
                sizeof(struct gui_msg_header) - headerReceived);
        if (bytesRead < 0) err(1, "read");
        headerReceived += bytesRead;
    }

    size_t length = headerBuffer.length;

    if (!messageBuffer) {
        messageBuffer = malloc(length);
        if (!messageBuffer) err(1, "malloc");
    }

    while (messageReceived < length) {
        ssize_t bytesRead = read(fd, messageBuffer + messageReceived,
                length - messageReceived);
        if (bytesRead < 0) err(1, "read");
        messageReceived += bytesRead;
    }

    void* msg = messageBuffer;
    messageBuffer = NULL;
    messageReceived = 0;
    headerReceived = 0;
    handleMessage(headerBuffer.type, msg);
    free(msg);
}

static void receiveMessages(void) {
    struct pollfd pfd[1];
    pfd[0].fd = fd;
    pfd[0].events = POLLIN;

    while (poll(pfd, 1, 0) == 1) {
        receiveMessage();
    }

    for (struct Window* win = firstWindow; win; win = win->next) {
        if (win->resized) {
            free(win->lfb);
            win->lfb = malloc(win->width * win->height * sizeof(uint32_t));
            if (!win->lfb) err(1, "malloc");
            redraw(win);
        }
    }
}

static void redraw(struct Window* window) {
    for (int y = 0; y < window->height; y++) {
        for (int x = 0; x < window->width; x++) {
            window->lfb[y * window->width + x] =
                    RGB(x % 255, y % 255, (x + y) % 255);
        }
    }

    drawText(window, 50, 50, 150, 30, "New Window");
    drawText(window, 50, 100, 150, 30, "New Client");
    drawText(window, 50, 150, 150, 30, window->textBox);

    struct gui_msg_redraw_window msg;
    msg.width = window->width;
    msg.height = window->height;
    msg.window_id = window->id;
    struct gui_msg_header header;
    header.type = GUI_MSG_REDRAW_WINDOW;
    size_t lfbSize = window->width * window->height * sizeof(uint32_t);
    header.length = sizeof(msg) + lfbSize;
    writeAll(fd, &header, sizeof(header));
    writeAll(fd, &msg, sizeof(msg));
    writeAll(fd, window->lfb, lfbSize);
}

static void redrawPart(struct Window* window, int x, int y, int width,
        int height) {
    if (x >= window->width || y >= window->height) return;
    if (x + width > window->width) {
        width = window->width - x;
    }
    if (y + height > window->height) {
        height = window->height - y;
    }

    struct gui_msg_redraw_window_part msg;
    msg.pitch = window->width;
    msg.x = x;
    msg.y = y;
    msg.width = width;
    msg.height = height;
    msg.window_id = window->id;
    struct gui_msg_header header;
    header.type = GUI_MSG_REDRAW_WINDOW_PART;
    size_t lfbSize = ((height - 1) * window->width + width) * sizeof(uint32_t);
    header.length = sizeof(msg) + lfbSize;
    writeAll(fd, &header, sizeof(header));
    writeAll(fd, &msg, sizeof(msg));
    writeAll(fd, window->lfb + y * window->width + x, lfbSize);
}

static void removeWindow(struct Window* window) {
    if (window->prev) {
        window->prev->next = window->next;
    } else {
        firstWindow = window->next;
    }

    if (window->next) {
        window->next->prev = window->prev;
    } else {
        lastWindow = window->prev;
    }

    free(window->lfb);
    free(window);
}

static void showWindow(struct Window* window) {
    struct gui_msg_show_window msg;
    msg.window_id = window->id;
    struct gui_msg_header header;
    header.type = GUI_MSG_SHOW_WINDOW;
    header.length = sizeof(msg);
    writeAll(fd, &header, sizeof(header));
    writeAll(fd, &msg, sizeof(msg));
}

static void writeAll(int fd, const void* buffer, size_t size) {
    const char* buf = buffer;
    size_t written = 0;
    while (written < size) {
        ssize_t bytesWritten = write(fd, buf + written, size - written);
        if (bytesWritten < 0) err(1, "write");
        written += bytesWritten;
    }
}

int main() {
    const char* socketPath = getenv("DENNIX_GUI_SOCKET");
    if (!socketPath) errx(1, "GUI is not running");

    loadFromFile("/share/fonts/vgafont", vgafont, sizeof(vgafont));

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if (strlen(socketPath) > sizeof(addr.sun_path)) {
        errx(1, "socket path too long");
    }
    strcpy(addr.sun_path, socketPath);

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) err(1, "socket");

    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err(1, "connect");
    }

    addWindow();

    while (1) {
        receiveMessages();
    }
}
