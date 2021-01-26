/* Copyright (c) 2021 Dennis Wölfing
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

/* kernel/include/dennix/kernel/endian.h
 * Endianness types.
 */

#ifndef KERNEL_ENDIAN_H
#define KERNEL_ENDIAN_H

#include <endian.h>
#include <dennix/kernel/kernel.h>

template <typename T, int E>
class Endian {
    static_assert(E == LITTLE_ENDIAN || E == BIG_ENDIAN,
            "Unsupported endianness");
    static_assert(BYTE_ORDER == LITTLE_ENDIAN || BYTE_ORDER == BIG_ENDIAN,
            "Unsupported endianness");
    static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
            "Unsupported type size");
private:
    static T convert(const T& value) {
        if (E == BYTE_ORDER) {
            return value;
        }

        if (sizeof(T) == 2) {
            return __bswap16(value);
        } else if (sizeof(T) == 4) {
            return __bswap32(value);
        } else if (sizeof(T) == 8) {
            return __bswap64(value);
        }
    }
public:
    Endian() {
        value = 0;
    }

    Endian(const T& val) {
        value = convert(val);
    }

    template<typename T2, int E2>
    Endian(const Endian<T2, E2>& other) {
        T2 val = other;
        value = convert(val);
    }

    operator T() const {
        return convert(value);
    }

    Endian& operator=(const T& other) {
        value = convert(other);
        return *this;
    }

    template<typename T2, int E2>
    Endian& operator=(const Endian<T2, E2>& other) {
        T2 val = other;
        value = convert(val);
        return *this;
    }
private:
T value;
};

typedef uint8_t little_uint8_t;
typedef Endian<uint16_t, LITTLE_ENDIAN> little_uint16_t;
typedef Endian<uint32_t, LITTLE_ENDIAN> little_uint32_t;
typedef Endian<uint64_t, LITTLE_ENDIAN> little_uint64_t;

typedef uint8_t big_uint8_t;
typedef Endian<uint16_t, BIG_ENDIAN> big_uint16_t;
typedef Endian<uint32_t, BIG_ENDIAN> big_uint32_t;
typedef Endian<uint64_t, BIG_ENDIAN> big_uint64_t;

#endif
