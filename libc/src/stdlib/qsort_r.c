/* Copyright (c) 2018, 2022, 2024 Dennis Wölfing
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

/* libc/src/stdlib/qsort_r.c
 * Sorting. (POSIX2024, called from C89)
 */

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
This is an implementation of the Smoothsort [1] algorithm. Smoothsort runs in
O(n) in the best case and O(n log n) in the average and worst case and uses
O(1) auxiliary space. It is similar to Heapsort but is based on a Leonardo
heap [2] instead on a binary heap.

The Leonardo numbers L(n) are defined as follows:
    L(0) = 1
    L(1) = 1
    L(n + 2) = L(n) + L(n + 1) + 1

A Leonardo tree Lt_k is a tree containing L(k) nodes. The trees Lt_0 and Lt_1
are singleton nodes and all other Lt_k trees consist of one root with two
subtrees, Lt_{k-1} and Lt_{k-2}.

A Leonardo heap is a sequence of trees (Lt_{k_1}, ..., Lt_{k_n}) which
satisfies these conditions:
- For all i: k_i > k_{i+1}
- In each tree each child node is less or equal to its parent.
- For all i: The root of a tree Lt_{k_i} is less or equal to the root of tree
  Lt_{k_{i+1}}

The algorithm constructs a Leonardo heap from the input and then repeatedly
removes the maximum element from the heap and restores the heap property until
all elements have been removed from the heap.

This implementation uses the same variable names as in Dijkstras paper [1].
For the definition of these variables, assume that Lt_k is the rightmost tree.
(k is not used directly in the algorithm and can be derived implicitly from b
and c)

N = total number of elements
m(i) = the i-th element of the array.
q = number of elements in the Leonardo heap.
p = bitvector where bit i means that Lt_{k+i} is part of the heap
r = index of the root of the rightmost tree of the heap
b = L(k)
c = L(k-1)

[1] Edsger W. Dijkstra, Smoothsort, an alternative for sorting in situ (1981)
    https://www.cs.utexas.edu/users/EWD/ewd07xx/EWD796a.PDF
[2] Keith Schwarz, Smoothsort Demystified (2011)
    http://www.keithschwarz.com/smoothsort/
*/

// We need a type twice as big as size_t.
#if 2 * __SIZEOF_SIZE_T__ <= __SIZEOF_LONG_LONG__
typedef unsigned long long p_type[1];

#  define equ(p, x) ((p)[0] == (x))
#  define shl(p, n) ((p)[0] <<= (n))
#  define shr(p, n) ((p)[0] >>= (n))
#else
typedef size_t p_type[2];

#  define equ(p, x) ((p)[1] == 0 && (p)[0] == (x))

static inline void shl(p_type p, size_t n) {
    p[1] <<= n;
    p[1] |= p[0] >> (sizeof(size_t) * CHAR_BIT - n);
    p[0] <<= n;
}

static inline void shr(p_type p, size_t n) {
    p[0] >>= n;
    p[0] |= p[1] << (sizeof(size_t) * CHAR_BIT - n);
    p[1] >>= n;
}
#endif

static void semitrinkle(void* base, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg,
        size_t b, size_t c, const p_type p, size_t r);
static void sift(void* base, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg,
        size_t b1, size_t c1, size_t r1);
static void swap(void* x, void* y, size_t width);
static void trinkle(void* base, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg,
        size_t b1, size_t c1, const p_type p, size_t r1);

#define m(i) ((void*)((uintptr_t) base + (i) * width))
#define up(b, c) do { size_t temp = b; b += c + 1; c = temp; } while (0)
#define down(b, c) do { size_t temp = b; b = c; c = temp - c - 1; } while (0)

void __qsort_r(void* base, size_t N, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg) {
    size_t q = 1;
    p_type p = {0};
    p[0] = 0x1;

    size_t r = 0;
    size_t b = 1;
    // c can have the value -1, we are using an unsigned type nevertheless
    // because it can also have values greater than SSIZE_MAX and it is never
    // used in a way that an underflow to SIZE_MAX would cause problems.
    size_t c = 1;

    while (q < N) {
        // Build the Leonardo heap.
        if ((p[0] & 0x7) == 0x3) {
            // The two smallest trees in the heap are Lt_k and Lt_{k+1}.
            // We merge both into a new tree Lt_{k+2} by adding a new node.
            sift(base, width, compare, arg, b, c, r);
            shr(p, 2);
            p[0] |= 0x1;
            up(b, c);
            up(b, c);
        } else {
            // Add the node as a Lt_1 or Lt_0 tree.
            if (q + c < N) {
                sift(base, width, compare, arg, b, c, r);
            } else {
                trinkle(base, width, compare, arg, b, c, p, r);
            }
            down(b, c);
            shl(p, 1);
            while (b > 1) {
                down(b, c);
                shl(p, 1);
            }
            p[0] |= 0x1;
        }

        q++;
        r++;
    }

    // Ensure that the heap property is satisfied.
    trinkle(base, width, compare, arg, b, c, p, r);

    while (q > 1) {
        // The root of the rightmost tree is the greatest remaining element.
        q--;

        if (b == 1) {
            r--;
            p[0] &= ~0x1;
            while (!(p[0] & 0x1)) {
                shr(p, 1);
                up(b, c);
            }
        } else {
            // Split the Lt_k tree into Lt_{k-1} and Lt_{k-2} trees.
            p[0] &= ~0x1;
            r = r - b + c;
            if (!equ(p, 0x0)) {
                semitrinkle(base, width, compare, arg, b, c, p, r);
            }

            down(b, c);
            shl(p, 1);
            p[0] |= 0x1;
            r += c;
            semitrinkle(base, width, compare, arg, b, c, p, r);
            down(b, c);
            shl(p, 1);
            p[0] |= 0x1;
        }
    }
}
__weak_alias(__qsort_r, qsort_r);

static void semitrinkle(void* base, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg,
        size_t b, size_t c, const p_type p, size_t r) {
    size_t r1 = r - c;
    if (compare(m(r1), m(r), arg) > 0) {
        swap(m(r), m(r1), width);
        trinkle(base, width, compare, arg, b, c, p, r1);
    }
}

static void sift(void* base, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg,
        size_t b1, size_t c1, size_t r1) {
    // Ensure that each child in the current tree is less or equal to its
    // parent.
    while (b1 >= 3) {
        size_t r2 = r1 - b1 + c1;
        // Choose the greater of the two children.
        if (compare(m(r2), m(r1 - 1), arg) <= 0) {
            r2 = r1 - 1;
            down(b1, c1);
        }
        if (compare(m(r1), m(r2), arg) >= 0) {
            return;
        } else {
            swap(m(r1), m(r2), width);
            r1 = r2;
            down(b1, c1);
        }
    }
}

static void swap(void* x, void* y, size_t width) {
    for (size_t i = 0; i < width; i++) {
        char temp = ((char*) x)[i];
        ((char*) x)[i] = ((char*) y)[i];
        ((char*) y)[i] = temp;
    }
}

static void trinkle(void* base, size_t width,
        int (*compare)(const void*, const void*, void*), void* arg,
        size_t b1, size_t c1, const p_type p, size_t r1) {
    p_type p1;
    memcpy(p1, p, sizeof(p_type));

    while (!equ(p1, 0)) {
        while (!(p1[0] & 0x1)) {
            shr(p1, 1);
            up(b1, c1);
        }
        size_t r3 = r1 - b1;

        if (equ(p1, 0x1) || compare(m(r3), m(r1), arg) <= 0) {
            break;
        } else {
            // The root of the tree left of the current tree greater than the
            // root of the current tree.
            p1[0] &= ~0x1;

            if (b1 == 1) {
                swap(m(r1), m(r3), width);
                r1 = r3;
            } else {
                size_t r2 = r1 - b1 + c1;
                // Choose the greater of the two children.
                if (compare(m(r2), m(r1 - 1), arg) <= 0) {
                    r2 = r1 - 1;
                    down(b1, c1);
                    shl(p1, 1);
                }

                // Either swap the current root with the root left from it or
                // with its greater child, whichever is greater.
                if (compare(m(r3), m(r2), arg) > 0) {
                    swap(m(r1), m(r3), width);
                    r1 = r3;
                } else {
                    swap(m(r1), m(r2), width);
                    r1 = r2;
                    down(b1, c1);
                    break;
                }
            }
        }
    }

    sift(base, width, compare, arg, b1, c1, r1);
}
