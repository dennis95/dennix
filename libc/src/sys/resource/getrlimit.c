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

/* libc/src/sys/resource/getrlimit.c
 * Resource limits.
 */

#include <errno.h>
#include <sys/resource.h>

// This is just a dummy implementation. We do not enforce any of these limits.

static struct rlimit core = { RLIM_INFINITY, RLIM_INFINITY };
static struct rlimit cpu = { RLIM_INFINITY, RLIM_INFINITY };
static struct rlimit data = { RLIM_INFINITY, RLIM_INFINITY };
static struct rlimit fsize = { RLIM_INFINITY, RLIM_INFINITY };
static struct rlimit nofile = { RLIM_INFINITY, RLIM_INFINITY };
static struct rlimit stack = { 128 * 1024, 128 * 1024 };
static struct rlimit as = { RLIM_INFINITY, RLIM_INFINITY };

int getrlimit(int resource, struct rlimit* limit) {
    switch (resource) {
    case RLIMIT_CORE: *limit = core; break;
    case RLIMIT_CPU: *limit = cpu; break;
    case RLIMIT_DATA: *limit = data; break;
    case RLIMIT_FSIZE: *limit = fsize; break;
    case RLIMIT_NOFILE: *limit = nofile; break;
    case RLIMIT_STACK: *limit = stack; break;
    case RLIMIT_AS: *limit = as; break;
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int setrlimit(int resource, const struct rlimit* limit) {
    struct rlimit* toset;
    switch (resource) {
    case RLIMIT_CORE: toset= &core; break;
    case RLIMIT_CPU: toset = &cpu; break;
    case RLIMIT_DATA: toset = &data; break;
    case RLIMIT_FSIZE: toset = &fsize; break;
    case RLIMIT_NOFILE: toset = &nofile; break;
    case RLIMIT_STACK: toset = &stack; break;
    case RLIMIT_AS: toset = &as; break;
    default:
        errno = EINVAL;
        return -1;
    }

    if (limit->rlim_max > toset->rlim_max) {
        errno = EPERM;
        return -1;
    }
    if (limit->rlim_cur > limit->rlim_max) {
        errno = EINVAL;
        return -1;
    }

    *toset = *limit;
    return 0;
}
