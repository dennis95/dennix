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

/* gui/gui.c
 * Graphical User Interface.
 */

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "display.h"
#include "gui.h"
#include "keyboard.h"
#include "mouse.h"
#include "server.h"

char vgafont[4096];

static void eventLoop(void);
static void initialize(void);

static void eventLoop(void) {
    pollEvents();

    if (damageRect.width != 0) {
        composit();
    }
}

static void initialize(void) {
    loadFromFile("/share/fonts/vgafont", vgafont, sizeof(vgafont));

    initializeKeyboard();
    initializeMouse();
    initializeDisplay();
    initializeServer();
}

void loadFromFile(const char* filename, void* buffer, size_t size) {
    FILE* file = fopen(filename, "r");
    if (!file) err(1, "cannot open '%s'", filename);
    size_t bytesRead = fread(buffer, 1, size, file);
    if (bytesRead < size) errx(1, "cannot load '%s", filename);
    fclose(file);
}

int main(void) {
    initialize();
    addDamageRect(displayRect);
    composit();

    pid_t pid = fork();
    if (pid == 0) {
        // Run the test application.
        execl("/bin/gui-test", "gui-test", NULL);
        _Exit(1);
    }

    while (true) {
        eventLoop();
    }
}
