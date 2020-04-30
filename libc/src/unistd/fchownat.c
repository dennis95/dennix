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

/* libc/src/unistd/fchownat.c
 * Change owner and group of a file.
 */

#include <unistd.h>
#include <dennix/fchownat.h>
#include <sys/syscall.h>

DEFINE_SYSCALL(SYSCALL_FCHOWNAT, int, sys_fchownat, (struct fchownatParams*));

int fchownat(int fd, const char* path, uid_t uid, gid_t gid, int flags) {
    struct fchownatParams params;
    params.fd = fd;
    params.path = path;
    params.uid = uid;
    params.gid = gid;
    params.flags = flags;
    return sys_fchownat(&params);
}
