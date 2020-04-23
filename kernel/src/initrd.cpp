/* Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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

#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <tar.h>
#include <dennix/kernel/file.h>
#include <dennix/kernel/initrd.h>
#include <dennix/kernel/panic.h>
#include <dennix/kernel/symlink.h>

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

Reference<DirectoryVnode> Initrd::loadInitrd(vaddr_t initrd) {
    Reference<DirectoryVnode> root = xnew DirectoryVnode(nullptr, 0755, 0);
    TarHeader* header = (TarHeader*) initrd;

    while (strcmp(header->magic, TMAGIC) == 0) {
        char* path;
        if (header->prefix[0]) {
            path = (char*) malloc(strnlen(header->name, sizeof(header->name)) +
                    strnlen(header->prefix, sizeof(header->prefix)) + 2);
            if (!path) PANIC("Allocation failure");

            stpcpy(stpcpy(stpcpy(path, header->prefix), "/"), header->name);
        } else {
            path = strndup(header->name, sizeof(header->name));
            if (!path) PANIC("Allocation failure");
        }

        char* path2 = strdup(path);
        if (!path2) PANIC("Allocation failure");
        char* dirName = dirname(path);
        char* fileName = basename(path2);

        Reference<DirectoryVnode> directory =
                (Reference<DirectoryVnode>) resolvePath(root, dirName);

        if (!directory) {
            PANIC("Could not add '%s' to nonexistent directory '%s'",
                    fileName, dirName);
        }

        Reference<Vnode> newFile;
        mode_t mode = (mode_t) strtol(header->mode, nullptr, 8);
        size_t size = (size_t) strtoul(header->size, nullptr, 8);
        struct timespec mtime;
        mtime.tv_sec = (time_t) strtoll(header->mtime, nullptr, 8);
        mtime.tv_nsec = 0;

        if (header->typeflag == REGTYPE || header->typeflag == AREGTYPE) {
            newFile = xnew FileVnode(header + 1, size, mode,
                    directory->stats.st_dev);
            header += 1 + ALIGNUP(size, 512) / 512;
        } else if (header->typeflag == DIRTYPE) {
            newFile = xnew DirectoryVnode(directory, mode,
                    directory->stats.st_dev);
            header++;
        } else if (header->typeflag == SYMTYPE) {
            newFile = xnew SymlinkVnode(header->linkname,
                    sizeof(header->linkname), directory->stats.st_dev);
            header++;
        } else if (header->typeflag == LNKTYPE) {
            char* linkname = strndup(header->linkname,
                    sizeof(header->linkname));
            if (!linkname || !(newFile = resolvePath(root, linkname))) {
                PANIC("Could not create symlink '/%s'", path);
            }
            free(linkname);
            header++;
        } else {
            PANIC("Unknown typeflag '%c'", header->typeflag);
        }

        newFile->stats.st_atim = mtime;
        newFile->stats.st_mtim = mtime;

        if (directory->link(fileName, newFile) < 0) {
            PANIC("Could not link file '/%s'", path);
        }
        free(path);
        free(path2);
    }

    return root;
}
