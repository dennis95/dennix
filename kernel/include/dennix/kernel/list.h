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

/* kernel/include/dennix/kernel/list.h
 * Linked list classes.
 */

#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H

#include <dennix/kernel/util.h>

template <typename T, T* T::*Next>
class SinglyLinkedList {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    class iterator {
    public:
        iterator() : ptr{nullptr} {}

        explicit iterator(pointer ptr) : ptr{ptr} {}

        iterator& operator++() {
            ptr = ptr->*Next;
            return *this;
        }

        reference operator*() const {
            return *ptr;
        }

        pointer operator->() const {
            return ptr;
        }

        bool operator==(const iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }

    private:
        pointer ptr;
    };

    class const_iterator {
    public:
        const_iterator() : ptr{nullptr} {}

        explicit const_iterator(const_pointer ptr) : ptr{ptr} {}

        const_iterator(const iterator& other) : ptr{other.ptr} {}

        const_iterator& operator++() {
            ptr = ptr->*Next;
            return *this;
        }

        const_reference operator*() const {
            return *ptr;
        }

        const_pointer operator->() const {
            return ptr;
        }

        bool operator==(const const_iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const const_iterator& other) const {
            return ptr != other.ptr;
        }

    private:
        const_pointer ptr;
    };

    SinglyLinkedList() = default;
    ~SinglyLinkedList() = default;
    NOT_COPYABLE(SinglyLinkedList);

    SinglyLinkedList(SinglyLinkedList&& other) : first{other.first} {
        other.first = nullptr;
    }

    SinglyLinkedList& operator=(SinglyLinkedList&& other) {
        first = other.first;
        other.first = nullptr;
        return *this;
    }

    iterator begin() {
        return iterator(first);
    }

    const_iterator begin() const {
        return const_iterator(first);
    }

    const_iterator cbegin() const {
        return const_iterator(first);
    }

    iterator end() {
        return iterator(nullptr);
    }

    const_iterator end() const {
        return const_iterator(nullptr);
    }

    const_iterator cend() const {
        return const_iterator(nullptr);
    }

    void addFront(reference value) {
        value.*Next = first;
        first = &value;
    }

    bool empty() const {
        return first == nullptr;
    }

    reference front() {
        return *first;
    }

    const_reference front() const {
        return *first;
    }

    void removeFront() {
        first = first->*Next;
    }

    void swap(SinglyLinkedList& other) {
        SinglyLinkedList tmp{Util::move(*this)};
        *this = Util::move(other);
        other = Util::move(tmp);
    }

private:
    pointer first = nullptr;
};

template <typename T, T* T::*Prev, T* T::*Next>
class LinkedList {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    class iterator {
    public:
        iterator() : ptr{nullptr} {}

        explicit iterator(pointer ptr) : ptr{ptr} {}

        iterator& operator++() {
            ptr = ptr->*Next;
            return *this;
        }

        iterator& operator--() {
            ptr = ptr->*Prev;
            return *this;
        }

        reference operator*() const {
            return *ptr;
        }

        pointer operator->() const {
            return ptr;
        }

        template <typename R>
        R& operator->*(R T::*p) const {
            return ptr->*p;
        }

        bool operator==(const iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }

    private:
        pointer ptr;
    };

    class const_iterator {
    public:
        const_iterator() : ptr{nullptr} {}

        explicit const_iterator(const_pointer ptr) : ptr{ptr} {}

        const_iterator(const iterator& other) : ptr{other.ptr} {}

        const_iterator& operator++() {
            ptr = ptr->*Next;
            return *this;
        }

        const_iterator& operator--() {
            ptr = ptr->*Prev;
            return *this;
        }

        const_reference operator*() const {
            return *ptr;
        }

        const_pointer operator->() const {
            return ptr;
        }

        bool operator==(const const_iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const const_iterator& other) const {
            return ptr != other.ptr;
        }

    private:
        const_pointer ptr;
    };

    LinkedList() = default;
    ~LinkedList() = default;
    NOT_COPYABLE(LinkedList);

    LinkedList(LinkedList&& other) : first{other.first} {
        other.first = nullptr;
    }

    LinkedList& operator=(LinkedList&& other) {
        first = other.first;
        other.first = nullptr;
        return *this;
    }

    iterator begin() {
        return iterator(first);
    }

    const_iterator begin() const {
        return const_iterator(first);
    }

    const_iterator cbegin() const {
        return const_iterator(first);
    }

    iterator end() {
        return iterator();
    }

    const_iterator end() const {
        return const_iterator();
    }

    const_iterator cend() const {
        return const_iterator();
    }

    void addAfter(iterator pos, reference value) {
        value.*Prev = &*pos;
        value.*Next = pos->*Next;
        pos->*Next = &value;
        if (value.*Next) {
            value.*Next->*Prev = &value;
        }
    }

    void addFront(reference value) {
        value.*Prev = nullptr;
        value.*Next = first;
        if (first) {
            first->*Prev = &value;
        }
        first = &value;
    }

    bool empty() const {
        return first == nullptr;
    }

    reference front() {
        return *first;
    }

    const_reference front() const {
        return *first;
    }

    void remove(reference object) {
        if (object.*Prev) {
            object.*Prev->*Next = object.*Next;
        } else {
            first = object.*Next;
        }

        if (object.*Next) {
            object.*Next->*Prev = object.*Prev;
        }

        object.*Prev = nullptr;
        object.*Next = nullptr;
    }

    void splice(iterator pos, LinkedList& source) {
        iterator sourceIter = source.begin();
        if (sourceIter == source.end()) return;

        if (pos != end() && pos->*Prev) {
            pos->*Prev->*Next = &*sourceIter;
            sourceIter->*Prev = pos->*Prev;
        } else {
            first = &*sourceIter;
        }

        iterator next = sourceIter;
        ++next;
        while (next != source.end()) {
            sourceIter = next;
            ++next;
        }

        if (pos != end()) {
            sourceIter->*Next = &*pos;
            pos->*Prev = &*sourceIter;
        }

        source.first = nullptr;
    }

    void swap(LinkedList& other) {
        LinkedList tmp{Util::move(*this)};
        *this = Util::move(other);
        other = Util::move(tmp);
    }

private:
    pointer first = nullptr;
};

template <typename T, T* T::*Prev, T* T::*Next>
class LinkedListWithEnd {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = typename LinkedList<T, Prev, Next>::iterator;
    using const_iterator = typename LinkedList<T, Prev, Next>::const_iterator;

    LinkedListWithEnd() = default;
    ~LinkedListWithEnd() = default;
    NOT_COPYABLE(LinkedListWithEnd);

    LinkedListWithEnd(LinkedListWithEnd&& other) : first{other.first},
            last{other.last} {
        other.first = nullptr;
        other.last = nullptr;
    }

    LinkedListWithEnd& operator=(LinkedListWithEnd&& other) {
        first = other.first;
        last = other.last;
        other.first = nullptr;
        other.last = nullptr;
        return *this;
    }

    iterator begin() {
        return iterator(first);
    }

    const_iterator begin() const {
        return const_iterator(first);
    }

    const_iterator cbegin() const {
        return const_iterator(first);
    }

    iterator end() {
        return iterator();
    }

    const_iterator end() const {
        return const_iterator();
    }

    const_iterator cend() const {
        return const_iterator();
    }

    void addAfter(iterator pos, reference value) {
        value.*Prev = &*pos;
        value.*Next = pos->*Next;
        pos->*Next = &value;
        if (value.*Next) {
            value.*Next->*Prev = &value;
        } else {
            last = &value;
        }
    }

    void addBack(reference value) {
        value.*Prev = last;
        value.*Next = nullptr;
        if (last) {
            last->*Next = &value;
        } else {
            first = &value;
        }
        last = &value;
    }

    void addFront(reference value) {
        value.*Prev = nullptr;
        value.*Next = first;
        if (first) {
            first->*Prev = &value;
        } else {
            last = &value;
        }
        first = &value;
    }

    bool empty() const {
        return first == nullptr;
    }

    reference front() {
        return *first;
    }

    const_reference front() const {
        return *first;
    }

    void remove(reference object) {
        if (object.*Prev) {
            object.*Prev->*Next = object.*Next;
        } else {
            first = object.*Next;
        }

        if (object.*Next) {
            object.*Next->*Prev = object.*Prev;
        } else {
            last = object.*Prev;
        }

        object.*Prev = nullptr;
        object.*Next = nullptr;
    }

    void swap(LinkedListWithEnd& other) {
        LinkedListWithEnd tmp{Util::move(*this)};
        *this = Util::move(other);
        other = Util::move(tmp);
    }

private:
    pointer first = nullptr;
    pointer last = nullptr;
};

#endif
