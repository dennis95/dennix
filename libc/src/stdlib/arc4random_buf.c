/* Copyright (c) 2020, 2022 Dennis Wölfing
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

/* libc/src/stdlib/arc4random_buf.c
 * Generate random bytes using the ChaCha20 algorithm. (called from C89)
 */

#ifndef __is_dennix_libk
#  define explicit_bzero __explicit_bzero
#  define getentropy __getentropy
#  define getpid __getpid
#  define pthread_mutex_lock __mutex_lock
#  define pthread_mutex_unlock __mutex_unlock
#endif
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __is_dennix_libk
void __lockRandom(void);
void __unlockRandom(void);
#else
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static inline uint32_t rotl(uint32_t value, unsigned int n) {
    return (value << n) | (value >> (32 - n));
}

#define QUARTERROUND(a, b, c, d) do { \
    a += b; d ^= a; d = rotl(d, 16); \
    c += d; b ^= c; b = rotl(b, 12); \
    a += b; d ^= a; d = rotl(d, 8); \
    c += d; b ^= c; b = rotl(b, 7); \
} while (0)

static void chacha20(const uint32_t state[16], uint32_t output[16]) {
    for (size_t i = 0; i < 16; i++) {
        output[i] = state[i];
    }
    for (size_t i = 0; i < 10; i++) {
        QUARTERROUND(output[0], output[4], output[8], output[12]);
        QUARTERROUND(output[1], output[5], output[9], output[13]);
        QUARTERROUND(output[2], output[6], output[10], output[14]);
        QUARTERROUND(output[3], output[7], output[11], output[15]);
        QUARTERROUND(output[0], output[5], output[10], output[15]);
        QUARTERROUND(output[1], output[6], output[11], output[12]);
        QUARTERROUND(output[2], output[7], output[8], output[13]);
        QUARTERROUND(output[3], output[4], output[9], output[14]);
    }
    for (size_t i = 0; i < 16; i++) {
        output[i] += state[i];
    }
}

static uint32_t state[16];

static void stir(void) {
    state[0] = 0x61707865;
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;

    uint32_t entropy[11];
    getentropy(entropy, sizeof(entropy));
    state[4] ^= entropy[0];
    state[5] ^= entropy[1];
    state[6] ^= entropy[2];
    state[7] ^= entropy[3];
    state[8] ^= entropy[4];
    state[9] ^= entropy[5];
    state[10] ^= entropy[6];
    state[11] ^= entropy[7];
    state[12] = 0;
    state[13] ^= entropy[8];
    state[14] ^= entropy[9];
    state[15] ^= entropy[10];
    explicit_bzero(entropy, sizeof(entropy));
}

void __arc4random_buf(void* result, size_t size) {
#ifdef __is_dennix_libk
    __lockRandom();
#else
    pthread_mutex_lock(&mutex);

    static pid_t pid = 0;
    if (pid != getpid()) {
        // TODO: This is not reliable because pids are reused in Dennix.
        pid = getpid();
        state[0] = 0;
    }
#endif

    uint32_t buffer[16];
    for (size_t i = 0; i < size; i += sizeof(buffer)) {
        if (!state[0] || state[12] >= 500000) {
            stir();
        }

        chacha20(state, buffer);
        size_t amount = size - i;
        if (amount > sizeof(buffer)) amount = sizeof(buffer);
        memcpy((char*) result + i, buffer, amount);
        state[12]++;
    }
    explicit_bzero(buffer, sizeof(buffer));

#ifdef __is_dennix_libk
    __unlockRandom();
#else
    pthread_mutex_unlock(&mutex);
#endif
}
__weak_alias(__arc4random_buf, arc4random_buf);
