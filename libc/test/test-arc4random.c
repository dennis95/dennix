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

#include "../src/stdlib/arc4random_buf.c"
#include "../src/stdlib/arc4random.c"
#include "../src/stdlib/arc4random_uniform.c"
#include <assert.h>

// These tests for the ChaCha20 implementation are based on the test vectors in
// RFC 8439.
static void testQuarterround(void) {
    uint32_t a = 0x11111111, b = 0x01020304, c = 0x9b8d6f43, d = 0x01234567;
    QUARTERROUND(a, b, c, d);
    assert(a == 0xea2a92f4);
    assert(b == 0xcb1cf8ce);
    assert(c == 0x4581472e);
    assert(d == 0x5881c4bb);
}

static void testChacha20(void) {
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c,
        0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c,
        0x00000001, 0x09000000, 0x4a000000, 0x00000000
    };
    uint32_t output[16];
    chacha20(state, output);
    assert(output[0] == 0xe4e7f110);
    assert(output[1] == 0x15593bd1);
    assert(output[2] == 0x1fdd0f50);
    assert(output[3] == 0xc47120a3);
    assert(output[4] == 0xc7f4d1c7);
    assert(output[5] == 0x0368c033);
    assert(output[6] == 0x9aaa2204);
    assert(output[7] == 0x4e6cd4c3);
    assert(output[8] == 0x466482d2);
    assert(output[9] == 0x09aa9f07);
    assert(output[10] == 0x05d7c214);
    assert(output[11] == 0xa2028bd9);
    assert(output[12] == 0xd19c12b5);
    assert(output[13] == 0xb94e16de);
    assert(output[14] == 0xe883d0cb);
    assert(output[15] == 0x4e3c50a2);
}

static void testUniform(void) {
    assert(arc4random_uniform(0) == 0);
    assert(arc4random_uniform(1) == 0);
    for (int i = 2; i < 10; i++) {
        for (int j = 0; j < 100; j++) {
            assert(arc4random_uniform(i) < i);
        }
    }
    for (int i = 0; i < 100; i++) {
        uint32_t value = arc4random();
        for (int j = 0; j < 100; j++) {
            assert(arc4random_uniform(value) < value);
        }
    }
}

int main(void) {
    testQuarterround();
    testChacha20();
    testUniform();
}
