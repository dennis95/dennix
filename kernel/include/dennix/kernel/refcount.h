/* Copyright (c) 2017, 2019 Dennis Wölfing
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

/* kernel/include/dennix/kernel/refcount.h
 * Reference counting.
 */

#ifndef KERNEL_REFCOUNT_H
#define KERNEL_REFCOUNT_H

#include <dennix/kernel/kernel.h>

class ReferenceCounted {
public:
    ReferenceCounted();
    virtual ~ReferenceCounted();
    void addReference() const;
    void removeReference() const;
private:
    mutable size_t refcount;
};

template <typename T>
class Reference {
public:
    Reference() : object(nullptr) {}

    Reference(T* obj) : object(obj) {
        if (object) {
            object->addReference();
        }
    }

    Reference(const Reference& ref) : Reference((T*) ref) {}

    // We want to ensure that an implicit cast from Reference<X> to
    // Reference<Y> is only possible when also an implicit cast from X* to Y*
    // is possible. To achieve this we provide a constructor for the implicit
    // conversion and a conversion operator for the explicit conversion.
    // To ensure that the correct one is called, we need some template magic to
    // exclude the implicitly converting constructor from the overload set when
    // an implicit cast is not allowed.
    template <typename T2, typename = decltype(Reference<T>((T2*) nullptr))>
    Reference(const Reference<T2>& ref) : Reference((T2*) ref) {}

    template <typename T2>
    explicit operator Reference<T2>() const {
        return Reference<T2>((T2*) object);
    }

    ~Reference() {
        if (object) {
            object->removeReference();
        }
    }

    Reference& operator=(const Reference& ref) {
        return operator=<T>(ref);
    }

    template <typename T2>
    Reference& operator=(const Reference<T2>& ref) {
        if (object == (T2*) ref) return *this;

        if (object) {
            object->removeReference();
        }
        object = (T2*) ref;
        if (object) {
            object->addReference();
        }

        return *this;
    }

    template <typename T2>
    bool operator==(const Reference<T2>& ref) {
        return object == (T2*) ref;
    }

    template <typename T2>
    bool operator==(T2* const& obj) {
        return object == obj;
    }

    template <typename T2>
    bool operator!=(const Reference<T2>& ref) {
        return object != (T2*) ref;
    }

    template <typename T2>
    bool operator!=(T2* const& obj) {
        return object != obj;
    }

    operator bool() const {
        return object;
    }

    explicit operator T*() const {
        return object;
    }

    T& operator*() const {
        return *object;
    }

    T* operator->() const {
        return object;
    }

private:
    T* object;
};

#endif
