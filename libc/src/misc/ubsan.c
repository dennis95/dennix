/* Copyright (c) 2022 Dennis Wölfing
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

/* libc/src/misc/ubsan.c
 * Undefined behavior sanitizer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#define ABORT_ALIAS(name) \
    extern __typeof(name) name ## _abort __attribute__((alias(#name)))

struct SourceLocation {
    const char* filename;
    uint32_t line;
    uint32_t column;
};

static noreturn void handleUndefinedBahavior(
        const struct SourceLocation* location, const char* message) {
    const char* filename = location->filename;
    if (!filename) filename = "(unknown)";
#ifdef __is_dennix_libc
    fprintf(stderr, "Undefined behavior at %s:%w32u:%w32u: %s\n",
            location->filename, location->line, location->column, message);
    abort();
#else
    extern noreturn void __handleUbsan(const char*, uint32_t, uint32_t,
            const char*);
    __handleUbsan(filename, location->line, location->column, message);
#endif
}

struct TypeMismatchData {
    struct SourceLocation loc;
    const void* type;
    unsigned char logAlignment;
    unsigned char typeCheckKind;
};

void __ubsan_handle_type_mismatch_v1(struct TypeMismatchData* data,
        uintptr_t ptr) {
    const char* message = "type mismatch";

    if (!ptr) {
        message = "null pointer access";
    } else if (ptr & (data->logAlignment - 1)) {
        message = "misaligned memory access";
    }

    handleUndefinedBahavior(&data->loc, message);
}
ABORT_ALIAS(__ubsan_handle_type_mismatch_v1);

struct OverflowData {
    struct SourceLocation loc;
    const void* type;
};

void __ubsan_handle_add_overflow(struct OverflowData* data, uintptr_t lhs,
        uintptr_t rhs) {
    (void) lhs; (void) rhs;
    handleUndefinedBahavior(&data->loc, "addition overflow");
}
ABORT_ALIAS(__ubsan_handle_add_overflow);

void __ubsan_handle_sub_overflow(struct OverflowData* data, uintptr_t lhs,
        uintptr_t rhs) {
    (void) lhs; (void) rhs;
    handleUndefinedBahavior(&data->loc, "subtraction overflow");
}
ABORT_ALIAS(__ubsan_handle_sub_overflow);

void __ubsan_handle_mul_overflow(struct OverflowData* data, uintptr_t lhs,
        uintptr_t rhs) {
    (void) lhs; (void) rhs;
    handleUndefinedBahavior(&data->loc, "multiplication overflow");
}
ABORT_ALIAS(__ubsan_handle_mul_overflow);

void __ubsan_handle_negate_overflow(struct OverflowData* data, uintptr_t val) {
    (void) val;
    handleUndefinedBahavior(&data->loc, "negation overflow");
}
ABORT_ALIAS(__ubsan_handle_negate_overflow);

void __ubsan_handle_divrem_overflow(struct OverflowData* data, uintptr_t lhs,
        uintptr_t rhs) {
    (void) lhs; (void) rhs;
    handleUndefinedBahavior(&data->loc, "division remainder overflow");
}
ABORT_ALIAS(__ubsan_handle_divrem_overflow);

struct ShiftOutOfBoundsData {
    struct SourceLocation loc;
    const void* lhsType;
    const void* rhsType;
};

void __ubsan_handle_shift_out_of_bounds(struct ShiftOutOfBoundsData* data,
        uintptr_t lhs, uintptr_t rhs) {
    (void) lhs; (void) rhs;
    handleUndefinedBahavior(&data->loc, "shift out of bounds");
}
ABORT_ALIAS(__ubsan_handle_shift_out_of_bounds);

struct OutOfBoundsData {
    struct SourceLocation loc;
    const void* arrayType;
    const void* indexType;
};

void __ubsan_handle_out_of_bounds(struct OutOfBoundsData* data,
        uintptr_t index) {
    (void) index;
    handleUndefinedBahavior(&data->loc, "Array access out of bounds");
}
ABORT_ALIAS(__ubsan_handle_out_of_bounds);

struct UnreachableData {
    struct SourceLocation loc;
};

void __ubsan_handle_builtin_unreachable(struct UnreachableData* data) {
    handleUndefinedBahavior(&data->loc, "unreachable code reached");
}

void __ubsan_handle_missing_return(struct UnreachableData* data) {
    handleUndefinedBahavior(&data->loc,
            "reached end of function without return");
}

struct VlaBoundData {
    struct SourceLocation loc;
    const void* type;
};

void __ubsan_handle_vla_bound_not_positive(struct VlaBoundData* data,
        uintptr_t bound) {
    (void) bound;
    handleUndefinedBahavior(&data->loc,
            "variable length array bound not positive");
}
ABORT_ALIAS(__ubsan_handle_vla_bound_not_positive);

struct FloatCastOverflowDataV2 {
    struct SourceLocation loc;
    const void* fromType;
    const void* toType;
};

void __ubsan_handle_float_cast_overflow(struct FloatCastOverflowDataV2* data,
        uintptr_t from) {
    (void) from;
    handleUndefinedBahavior(&data->loc, "float cast overflow");
}
ABORT_ALIAS(__ubsan_handle_float_cast_overflow);

struct InvalidValueData {
    struct SourceLocation loc;
    const void* type;
};

void __ubsan_handle_load_invalid_value(struct InvalidValueData* data,
        uintptr_t val) {
    (void) val;
    handleUndefinedBahavior(&data->loc, "invalid value loaded");
}
ABORT_ALIAS(__ubsan_handle_load_invalid_value);

struct InvalidBuiltinData {
    struct SourceLocation loc;
    unsigned char kind;
};

void __ubsan_handle_invalid_builtin(struct InvalidBuiltinData* data) {
    handleUndefinedBahavior(&data->loc, "invalid value passed to builtin");
}
ABORT_ALIAS(__ubsan_handle_invalid_builtin);

struct NonNullReturnData {
    struct SourceLocation attrLoc;
};

void __ubsan_handle_nonnull_return_v1(struct NonNullReturnData* data,
        struct SourceLocation* locPtr) {
    (void) data;
    handleUndefinedBahavior(locPtr, "Nonnull function returned null");
}
ABORT_ALIAS(__ubsan_handle_nonnull_return_v1);

struct NonNullArgData {
    struct SourceLocation loc;
    struct SourceLocation attrLoc;
    int argIndex;
};

void __ubsan_handle_nonnull_arg(struct NonNullArgData* data) {
    handleUndefinedBahavior(&data->loc, "Nonnull argument was null");
}
ABORT_ALIAS(__ubsan_handle_nonnull_arg);

struct PointerOverflowData {
    struct SourceLocation loc;
};

void __ubsan_handle_pointer_overflow(struct PointerOverflowData* data,
        uintptr_t base, uintptr_t result) {
    (void) base; (void) result;
    handleUndefinedBahavior(&data->loc, "pointer overflow");
}
ABORT_ALIAS(__ubsan_handle_pointer_overflow);

struct DynamicTypeCacheMissData {
    struct SourceLocation loc;
    const void* type;
    void* typeInfo;
    unsigned char typeCheckKind;
};

void __ubsan_handle_dynamic_type_cache_miss(
        struct DynamicTypeCacheMissData* data, uintptr_t ptr, uintptr_t hash) {
    (void) data; (void) ptr; (void) hash;
    // Ubsan expects us to perform dynamic type checking to check whether this
    // is really undefined behavior. We do not implement this check and thus
    // ignore this call.
}
ABORT_ALIAS(__ubsan_handle_dynamic_type_cache_miss);
