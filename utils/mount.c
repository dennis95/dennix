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

/* utils/mount.c
 * Mount a filesystem.
 */

#include "utils.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <sys/fs.h>

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "read-only", no_argument, 0, 'r' },
        { "read-write", no_argument, 0, 'w' },
        { "rw", no_argument, 0, 'w' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool forceWrite = false;
    int mountFlags = 0;

    int c;
    while ((c = getopt_long(argc, argv, "rw", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] FILE MOUNTPOINT\n"
                    "  -r, --read-only          mount readonly\n"
                    "  -w, --rw, --read-write   force mount as writable\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'r':
            forceWrite = false;
            mountFlags |= MOUNT_READONLY;
            break;
        case 'w':
            forceWrite = true;
            mountFlags &= ~MOUNT_READONLY;
            break;
        case '?':
            return 1;
        }
    }

    if (optind >= argc) errx(1, "missing file operand");
    if (optind == argc - 1) errx(1, "missing mountpoint operand");
    const char* file = argv[optind];
    const char* mountPoint = argv[optind + 1];

    if (mount(file, mountPoint, "ext234", mountFlags) < 0) {
        if (!forceWrite && errno == EROFS && !(mountFlags & MOUNT_READONLY)) {
            mountFlags |= MOUNT_READONLY;
            if (mount(file, mountPoint, "ext234", mountFlags) < 0) {
                err(1, "failed to mount '%s'", file);
            }
            warnx("'%s' is not writable, mounted readonly", file);
        } else {
            err(1, "failed to mount '%s'", file);
        }
    }
}
