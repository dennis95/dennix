/* Copyright (c) 2016, 2017, 2018, 2019, 2023 Dennis Wölfing
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

/* kernel/include/dennix/kernel/kernel.h
 * Common definitions for the kernel.
 */

#ifndef KERNEL_KERNEL_H
#define KERNEL_KERNEL_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define ALIGNED(alignment) __attribute__((__aligned__(alignment)))
#define NORETURN __attribute__((__noreturn__))
#define PACKED __attribute__((__packed__))
#define restrict __restrict
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#define PRINTF_LIKE(format, firstArg) \
    __attribute__((__format__(__printf__, format, firstArg)))

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ALIGNUP(val, alignment) ((((val) - 1) & ~((alignment) - 1)) + (alignment))
#define PAGE_MISALIGN (PAGESIZE - 1)
#define PAGE_ALIGNED(value) !((value) & PAGE_MISALIGN)

#define NOT_COPYABLE(T) \
    T(const T&) = delete; \
    T& operator=(const T&) = delete
#define NOT_MOVABLE(T) \
    T(T&&) = delete; \
    T& operator=(T&&) = delete

// Define an incomplete type for symbols so we can only take their addresses
typedef struct _incomplete_type symbol_t;

typedef uintptr_t paddr_t;
typedef uintptr_t vaddr_t;

// Placement new
inline void* operator new(size_t /*size*/, void* addr) {
    return addr;
}
inline void* operator new[](size_t /*size*/, void* addr) {
    return addr;
}
inline void operator delete(void*, void*) {}
inline void operator delete[](void*, void*) {}

class ConstructorMayFail {
public:
    bool __constructionFailed = false;

    // Disallow placement new.
    void* operator new(size_t size) { return ::operator new(size); }
};

#define FAIL_CONSTRUCTOR do { __constructionFailed = true; return; } while (0)

NORETURN void panic(const char* file, unsigned int line, const char* func,
        const char* format, ...) PRINTF_LIKE(4, 5);

class __new {
private:
    // This is just needed for the template magic below.
    struct YesType { int yes; };
    struct NoType { int no; };
    static YesType test(ConstructorMayFail*);
    static NoType test(void*);
public:
    __new() : file(nullptr) {}
    __new(const char* file, unsigned int line, const char* func) : file(file),
            line(line), func(func) {}
    const __new& operator*(const __new&) const { return *this; }

    template <typename T, typename = decltype(test((T*) nullptr).yes)>
    ALWAYS_INLINE T* operator*(T* ptr) const {
        if (unlikely(file && !ptr)) {
            panic(file, line, func, "Allocation failure");
        } else if (unlikely(file && ptr->__constructionFailed)) {
            panic(file, line, func, "Construction failure");
        } else if (unlikely(ptr && ptr->__constructionFailed)) {
            delete ptr;
            return nullptr;
        }
        return ptr;
    }

    template <typename T, typename = decltype(test((T*) nullptr).no),
            typename = T>
    ALWAYS_INLINE T* operator*(T* ptr) const {
        if (unlikely(file && !ptr)) {
            panic(file, line, func, "Allocation failure");
        }
        return ptr;
    }
private:
    const char* file;
    unsigned int line;
    const char* func;
};

// Behaves mostly like the normal new keyword but also returns nullptr if the
// constructor fails.
#define new __new() * new
// Behaves like new but causes a kernel panic on failure.
#define xnew __new(__FILE__, __LINE__, __func__) * new

#endif
