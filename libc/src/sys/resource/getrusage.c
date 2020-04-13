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

/* libc/src/sys/resource/getrusage.c
 * Get resource usage.
 */

#include <sys/resource.h>

int getrusage(int who, struct rusage* usage) {
    struct rusagens usagens;
    int result = getrusagens(who, &usagens);
    if (result < 0) return -1;
    usage->ru_utime.tv_sec = usagens.ru_utime.tv_sec;
    usage->ru_utime.tv_usec = usagens.ru_utime.tv_nsec / 1000;
    usage->ru_stime.tv_sec = usagens.ru_stime.tv_sec;
    usage->ru_stime.tv_usec = usagens.ru_stime.tv_nsec / 1000;
    return result;
}
