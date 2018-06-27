/* Copyright (c) 2017, 2018 Dennis Wölfing
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

/* kernel/src/symlink.cpp
 * Symbolic links.
 */

#include <stdlib.h>
#include <string.h>
#include <dennix/kernel/symlink.h>

SymlinkVnode::SymlinkVnode(const char* target, dev_t dev)
        : Vnode(S_IFLNK | 0777, dev) {
    this->target = strdup(target);
    stats.st_size = strlen(target);
}

SymlinkVnode::SymlinkVnode(const char* target, size_t targetLength, dev_t dev)
        : Vnode(S_IFLNK | 0777, dev) {
    this->target = strndup(target, targetLength);
    stats.st_size = strlen(this->target);
}

SymlinkVnode::~SymlinkVnode() {
    free((char*) target);
}

char* SymlinkVnode::getLinkTarget() {
    return strdup(target);
}
