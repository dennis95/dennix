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

/* kernel/src/initrd.cpp
 * Initial RAM disk.
 */

#include <stdlib.h>
#include <string.h>
#include <tar.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/initrd.h>
#include <dennix/kernel/log.h>

struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

DirectoryVnode* Initrd::loadInitrd(vaddr_t initrd) {
    DirectoryVnode* root = new DirectoryVnode();
    TarHeader* header = (TarHeader*) initrd;

    while (strcmp(header->magic, TMAGIC) == 0) {
        // TODO: Add support for subdirectories

        size_t size = (size_t) strtoul(header->size, NULL, 10);

        if (header->typeflag == REGTYPE || header->typeflag == AREGTYPE) {
            FileVnode* file = new FileVnode(header + 1, size);
            root->addChildNode(strdup(header->name), file);

            Log::printf("File: /%s, size = %zu\n", header->name, size);
        }

        header += 1 + ALIGNUP(size, 512) / 512;
    }

    return root;
}
