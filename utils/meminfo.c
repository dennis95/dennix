/* Copyright (c) 2020, 2021 Dennis Wölfing
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

/* utils/meminfo.c
 * Print memory information.
 */

#include <stdio.h>
#include <unistd.h>

int main(void) {
    struct meminfo info;
    meminfo(&info);
    size_t used = info.mem_total - info.mem_available;
    size_t cached = info.mem_available - info.mem_free;
    printf("total:     %9zu KiB\nused:      %9zu KiB\navailable: %9zu KiB\n"
            "free:      %9zu KiB\ncached:    %9zu KiB\n",
            info.mem_total / 1024, used / 1024, info.mem_available / 1024,
            info.mem_free / 1024, cached / 1024);
}
