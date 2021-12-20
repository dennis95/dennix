/* Copyright (c) 2018, 2019, 2020, 2021 Dennis Wölfing
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

/* sh/tokenizer.c
 * Token recognition.
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdlib.h>

#include "tokenizer.h"

static bool canBeginOperator(char c);
static bool canContinueOperator(const char* s, size_t opLength, char c);
static void delimit(struct Tokenizer* tokenizer, enum TokenType type);
static void nest(struct Tokenizer* tokenizer, enum TokenStatus status);
static void unnest(struct Tokenizer* tokenizer);

void initTokenizer(struct Tokenizer* tokenizer) {
    tokenizer->backslash = false;
    tokenizer->numTokens = 0;
    tokenizer->prev = NULL;
    tokenizer->tokens = NULL;
    tokenizer->tokenStatus = TOKEN_TOPLEVEL;
    tokenizer->wordStatus = WORDSTATUS_NONE;

    initStringBuffer(&tokenizer->buffer);
}

static inline bool isShellOrCommandSubstitution(struct Tokenizer* tokenizer) {
    return tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
            tokenizer->tokenStatus == TOKEN_COMMAND_SUBS;
}

enum TokenizerResult splitTokens(struct Tokenizer* tokenizer,
        const char* input) {
    // Handle end of file.
    if (!*input) {
        if (tokenizer->tokenStatus != TOKEN_TOPLEVEL) {
            return TOKENIZER_PREMATURE_EOF;
        }

        if (tokenizer->wordStatus != WORDSTATUS_NONE) {
            enum TokenType type = tokenizer->wordStatus == WORDSTATUS_OPERATOR ?
                    OPERATOR : TOKEN;
            delimit(tokenizer, type);
        }

        // Append a newline token.
        appendToStringBuffer(&tokenizer->buffer, '\n');
        delimit(tokenizer, OPERATOR);

        return TOKENIZER_DONE;
    }

    while (*input) {
        char c = *input++;

        if (tokenizer->tokenStatus == TOKEN_COMMENT) {
            if (c == '\n') {
                unnest(tokenizer);
            } else {
                continue;
            }
        }

        bool escaped = tokenizer->backslash;
        tokenizer->backslash = false;

        if (escaped && c == '\n') {
            tokenizer->buffer.used--;
            continue;
        }

        if (!escaped && tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED &&
                c == '\\') {
            tokenizer->backslash = true;
            goto appendAndNext;
        }

        if (tokenizer->wordStatus == WORDSTATUS_NUMBER &&
                ((!isdigit(c) && c != '<' && c != '>') || escaped)) {
            tokenizer->wordStatus = WORDSTATUS_WORD;
        }

        if (tokenizer->wordStatus == WORDSTATUS_OPERATOR) {
            if (!escaped && canContinueOperator(tokenizer->buffer.buffer,
                    tokenizer->buffer.used, c)) {
                goto appendAndNext;
            } else {
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL) {
                    delimit(tokenizer, OPERATOR);
                }
                tokenizer->wordStatus = WORDSTATUS_NONE;
            }
        }

        if (!escaped) {
            if (tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED &&
                    tokenizer->tokenStatus != TOKEN_DOUBLE_QUOTED &&
                    c == '\'') {
                nest(tokenizer, TOKEN_SINGLE_QUOTED);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_SINGLE_QUOTED && c == '\'') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus != TOKEN_DOUBLE_QUOTED &&
                    tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED &&
                    c == '"') {
                nest(tokenizer, TOKEN_DOUBLE_QUOTED);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_DOUBLE_QUOTED && c == '"') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED && c == '$') {
                tokenizer->wordStatus = WORDSTATUS_DOLLAR_SIGN;
                goto appendAndNext;
            }

            if (tokenizer->wordStatus == WORDSTATUS_DOLLAR_SIGN) {
                if (c == '{') {
                    nest(tokenizer, TOKEN_PARAMETER_EXP);
                    goto appendAndNext;
                } else if (c == '(') {
                    nest(tokenizer, TOKEN_COMMAND_SUBS);
                    goto appendAndNext;
                } else {
                    tokenizer->wordStatus = WORDSTATUS_WORD;
                }
            }

            if (tokenizer->tokenStatus == TOKEN_PARAMETER_EXP && c == '}') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_COMMAND_SUBS &&  c == ')') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus != TOKEN_BACKTICK && c == '`') {
                nest(tokenizer, TOKEN_BACKTICK);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_BACKTICK && c == '`') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (isShellOrCommandSubstitution(tokenizer)
                    && canBeginOperator(c)) {
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL) {
                    enum TokenType type = (tokenizer->wordStatus ==
                            WORDSTATUS_NUMBER) ? IO_NUMBER : TOKEN;
                    delimit(tokenizer, type);
                }

                tokenizer->wordStatus = WORDSTATUS_OPERATOR;
                goto appendAndNext;
            }

            if (isShellOrCommandSubstitution(tokenizer) && isblank(c)) {
                tokenizer->wordStatus = WORDSTATUS_NONE;
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL) {
                    delimit(tokenizer, TOKEN);
                    continue;
                }
                goto appendAndNext;
            }
        }

        if (isShellOrCommandSubstitution(tokenizer) &&
                tokenizer->wordStatus == WORDSTATUS_NONE && c == '#') {
            nest(tokenizer, TOKEN_COMMENT);
            continue;
        }

        if (tokenizer->wordStatus == WORDSTATUS_NONE) {
            if (isdigit(c) && !escaped) {
                tokenizer->wordStatus = WORDSTATUS_NUMBER;
            } else {
                tokenizer->wordStatus = WORDSTATUS_WORD;
            }
        }

appendAndNext:
        appendToStringBuffer(&tokenizer->buffer, c);
    }

    if (tokenizer->tokenStatus != TOKEN_TOPLEVEL) {
        return TOKENIZER_NEED_INPUT;
    }

    if (tokenizer->wordStatus == WORDSTATUS_OPERATOR &&
            tokenizer->buffer.used == 1 &&
            tokenizer->buffer.buffer[0] == '\n') {
        delimit(tokenizer, OPERATOR);
        return TOKENIZER_DONE;
    }

    return TOKENIZER_NEED_INPUT;
}

void freeTokenizer(struct Tokenizer* tokenizer) {
    for (size_t i = 0; i < tokenizer->numTokens; i++) {
        free(tokenizer->tokens[i].text);
    }
    free(tokenizer->tokens);
    free(tokenizer->buffer.buffer);
}

static bool canBeginOperator(char c) {
    return c == '\n' || c == '&' || c == '(' || c == ')' || c == ';'
            || c == '<' || c == '>' || c == '|';
}

static bool canContinueOperator(const char* s, size_t opLength, char c) {
    if (opLength != 1) {
        return opLength == 2 && s[0] == '<' && s[1] == '<' && c == '-';
    }

    if (s[0] == '&') {
        return c == '&';
    } else if (s[0] == ';') {
        return c == ';' || c == '&';
    } else if (s[0] == '<') {
        return c == '<' || c == '&' || c == '>';
    } else if (s[0] == '>') {
        return c == '>' || c == '&' || c == '|';
    } else if (s[0] == '|') {
        return c == '|';
    }

    return false;
}

static void delimit(struct Tokenizer* tokenizer, enum TokenType type) {
    assert(tokenizer->tokenStatus == TOKEN_TOPLEVEL);

    if (tokenizer->buffer.used == 0) return;

    struct Token token;
    token.type = type;
    token.text = finishStringBuffer(&tokenizer->buffer);
    addToArray((void**) &tokenizer->tokens, &tokenizer->numTokens,
            &token, sizeof(struct Token));
    initStringBuffer(&tokenizer->buffer);
}

static void nest(struct Tokenizer* tokenizer, enum TokenStatus status) {
    tokenizer->wordStatus = WORDSTATUS_NONE;
    struct TokenizerContext* prev = malloc(sizeof(struct TokenizerContext));
    if (!prev) err(1, "malloc");
    prev->prev = tokenizer->prev;
    prev->tokenStatus = tokenizer->tokenStatus;
    tokenizer->prev = prev;
    tokenizer->tokenStatus = status;
}

static void unnest(struct Tokenizer* tokenizer) {
    assert(tokenizer->prev);
    tokenizer->wordStatus = WORDSTATUS_WORD;
    struct TokenizerContext* prev = tokenizer->prev;
    tokenizer->tokenStatus = prev->tokenStatus;
    tokenizer->prev = prev->prev;
    free(prev);
}
