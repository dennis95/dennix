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

/* libc/src/stdlib/arc4random_uniform.c
 * Generate a random number.
 */

#include <stdlib.h>

uint32_t arc4random_uniform(uint32_t upperBound) {
    if (upperBound <= 1) return 0;

    // If we simply did a modulo upperBound we would get biased numbers if
    // upperBound is not a power of two. Specifically if N is
    // floor(2^32 / upperBound) then numbers less than 2^32 % upperBound would
    // have a probability of (N + 1) / 2^32 and all other numbers less than
    // upperBound have a probability of N / 2^32. Therefore we keep generating
    // random numbers until the value is outside of the biased range and then do
    // the modulo, giving each number the same probability.

    uint32_t value;
    do {
        value = arc4random();
    } while (value < -upperBound % upperBound);

    return value % upperBound;
}
