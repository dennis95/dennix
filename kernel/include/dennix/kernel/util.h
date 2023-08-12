/* Copyright (c) 2023 Dennis Wölfing
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

/* kernel/include/dennix/kernel/util.h
 * Utility functions.
 */

#ifndef KERNEL_UTIL_H
#define KERNEL_UTIL_H

#include <dennix/kernel/kernel.h>

namespace Util {

template <typename T>
struct remove_reference {
    typedef T type;
};

template <typename T>
struct remove_reference<T&> {
    typedef T type;
};

template <typename T>
struct remove_reference<T&&> {
    typedef T type;
};

template <typename Container>
bool containsOnly(const Container& container,
        typename Container::const_reference element) {
    auto iter = container.begin();
    return iter != container.end() && *iter == element &&
            ++iter == container.end();
}

template <typename Iterator, typename Predicate>
Iterator findIf(Iterator begin, Iterator end, Predicate pred) {
    while (begin != end) {
        if (pred(*begin)) {
            return begin;
        }
        ++begin;
    }

    return end;
}

template <typename T>
typename remove_reference<T>::type&& move(T&& obj) {
    return static_cast<typename remove_reference<T>::type&&>(obj);
}

template <typename Iterator>
Iterator next(Iterator iter) {
    return ++iter;
}

}

#endif
