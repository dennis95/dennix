/* Copyright (c) 2018, 2019, 2020, 2022 Dennis Wölfing
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

/* sh/tokenizer.h
 * Token recognition.
 */

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "stringbuffer.h"

enum TokenType {
    OPERATOR,
    IO_NUMBER,
    TOKEN,
};

struct Token {
    enum TokenType type;
    char* text;
};

enum TokenStatus {
    TOKEN_TOPLEVEL,
    TOKEN_DOUBLE_QUOTED,
    TOKEN_PARAMETER_EXP,

    TOKEN_COMMENT,
    TOKEN_SINGLE_QUOTED,
    TOKEN_BACKTICK,
    TOKEN_EOF,
};

enum WordStatus {
    WORDSTATUS_NONE,
    WORDSTATUS_WORD,
    WORDSTATUS_OPERATOR,
    WORDSTATUS_DOLLAR_SIGN,
    WORDSTATUS_NUMBER,
    WORDSTATUS_HERE_DOC,
};

enum TokenizerResult {
    TOKENIZER_DONE,
    TOKENIZER_PREMATURE_EOF,
    TOKENIZER_SYNTAX_ERROR,
};

struct TokenizerContext {
    struct TokenizerContext* prev;
    enum TokenStatus tokenStatus;
};

struct HereDoc {
    char* content;
    char* delimiter;
    bool stripTabs;
};

struct Tokenizer {
    bool backslash;
    struct StringBuffer buffer;
    size_t numTokens;
    struct TokenizerContext* prev;
    struct Token* tokens;
    size_t numHereDocs;
    struct HereDoc* hereDocs;
    enum TokenStatus tokenStatus;
    enum WordStatus wordStatus;
    const char* input;
    bool (*readInput)(const char** str, bool newCommand, void* context);
    void* context;
};

void initTokenizer(struct Tokenizer* tokenizer,
        bool (*readInput)(const char** str, bool newCommand, void* context),
        void* context);
enum TokenizerResult splitTokens(struct Tokenizer* tokenizer);
void freeTokenizer(struct Tokenizer* tokenizer);

#endif
