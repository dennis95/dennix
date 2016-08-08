/* Copyright (c) 2016, Dennis Wölfing
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

/* utils/test.c
 * Some program to test program loading.
 */

#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    (void) argc; (void) argv;
    printf("Hello %s from userspace!\n", "World");

    char buffer[81];

    while (1) {
        fgets(buffer, sizeof (buffer), stdin);

        // Remove the trailing newline
        size_t length = strlen(buffer);
        if (buffer[length - 1] == '\n') {
            buffer[length - 1] = '\0';
        }

        if (strcmp(buffer, "exit") == 0) {
            puts("Exiting.");
            return 42;
        }

        FILE* file = fopen(buffer, "r");

        if (!file) {
            printf("Failed to open file '%s'\n", buffer);
            continue;
        }

        while (fgets(buffer, sizeof(buffer), file)) {
            fputs(buffer, stdout);
        }

        fclose(file);
    }
}
