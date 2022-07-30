/* Copyright (c) 2018, 2019, 2020, 2021, 2022 Dennis Wölfing
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

/* sh/expand.h
 * Word expansion.
 */

#ifndef EXPAND_H
#define EXPAND_H

#include "sh.h"

struct SubstitutionInfo {
    size_t begin;
    size_t end;
    size_t startField;
    size_t endField;
    bool applyFieldSplitting;
    bool splitAtEnd;
};

struct ExpandContext {
    struct SubstitutionInfo* substitutions;
    size_t numSubstitutions;
    int flags;
    bool deleteIfEmpty;
    char* temp;
};

enum {
    EXPAND_NO_FIELD_SPLIT = 1 << 0,
    EXPAND_PATHNAMES = 1 << 1,
    EXPAND_NO_QUOTES = 1 << 2,
};

NO_DISCARD ssize_t expand(const char* word, int flags, char*** result);
NO_DISCARD ssize_t expand2(const char* word, int flags, char*** fields,
        struct ExpandContext* context);
char* expandWord(const char* word);
char* expandWord2(const char* word, int flags);
char* removeQuotes(const char* word, size_t fieldIndex,
        struct SubstitutionInfo* substitutions, size_t numSubstitutions,
        bool backslashOnly);

#endif
