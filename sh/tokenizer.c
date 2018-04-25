/* Copyright (c) 2018 Dennis Wölfing
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
#include <stdlib.h>

#include "tokenizer.h"

NO_DISCARD static bool addToken(struct Tokenizer* tokenizer,
        enum TokenType type, char* text);
static bool canBeginOperator(char c);
static bool canContinueOperator(const char* s, size_t opLength, char c);
NO_DISCARD static bool delimit(struct Tokenizer* tokenizer,
        enum TokenType type);
NO_DISCARD static bool nest(struct Tokenizer* tokenizer,
        enum TokenStatus status);
static void unnest(struct Tokenizer* tokenizer);

bool initTokenizer(struct Tokenizer* tokenizer) {
    tokenizer->backslash = false;
    tokenizer->numTokens = 0;
    tokenizer->prev = NULL;
    tokenizer->tokens = NULL;
    tokenizer->tokenStatus = TOKEN_TOPLEVEL;
    tokenizer->wordStatus = WORDSTATUS_NONE;

    return initStringBuffer(&tokenizer->buffer);
}

static inline bool isShellOrCommandSubstitution(struct Tokenizer* tokenizer) {
    return tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
            tokenizer->tokenStatus == TOKEN_SUBSHELL ||
            tokenizer->tokenStatus == TOKEN_COMMAND_SUBS ||
            tokenizer->tokenStatus == TOKEN_CMD_SUBSHELL;
}

enum TokenizerResult splitTokens(struct Tokenizer* tokenizer,
        const char* input) {
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
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
                        tokenizer->tokenStatus == TOKEN_SUBSHELL) {
                    if (!delimit(tokenizer, OPERATOR)) return TOKENIZER_ERROR;
                }
                tokenizer->wordStatus = WORDSTATUS_NONE;
            }
        }

        if (!escaped) {
            if (tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED && c == '\'') {
                if (!nest(tokenizer, TOKEN_SINGLE_QUOTED)) {
                    return TOKENIZER_ERROR;
                }
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_SINGLE_QUOTED && c == '\'') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus != TOKEN_DOUBLE_QUOTED &&
                    tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED &&
                    c == '"') {
                if (!nest(tokenizer, TOKEN_DOUBLE_QUOTED)) {
                    return TOKENIZER_ERROR;
                }
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
                    if (!nest(tokenizer, TOKEN_PARAMETER_EXP)) {
                        return TOKENIZER_ERROR;
                    }
                    goto appendAndNext;
                } else if (c == '(') {
                    if (!nest(tokenizer, TOKEN_COMMAND_SUBS)) {
                        return TOKENIZER_ERROR;
                    }
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
                if (!nest(tokenizer, TOKEN_BACKTICK)) return TOKENIZER_ERROR;
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_BACKTICK && c == '`') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (isShellOrCommandSubstitution(tokenizer) && c == '(') {
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
                        tokenizer->tokenStatus == TOKEN_SUBSHELL) {
                    if (!delimit(tokenizer, OPERATOR)) return TOKENIZER_ERROR;
                    if (!nest(tokenizer, TOKEN_SUBSHELL)) {
                        return TOKENIZER_ERROR;
                    }
                } else if (tokenizer->tokenStatus == TOKEN_COMMAND_SUBS ||
                        tokenizer->tokenStatus == TOKEN_CMD_SUBSHELL) {
                    if (!nest(tokenizer, TOKEN_CMD_SUBSHELL)) {
                        return TOKENIZER_ERROR;
                    }
                }

                tokenizer->wordStatus = WORDSTATUS_OPERATOR;
                goto appendAndNext;
            }

            if ((tokenizer->tokenStatus == TOKEN_SUBSHELL ||
                    tokenizer->tokenStatus == TOKEN_CMD_SUBSHELL)
                    && c == ')') {
                if (tokenizer->tokenStatus == TOKEN_SUBSHELL) {
                    if (!delimit(tokenizer, OPERATOR)) return TOKENIZER_ERROR;
                }
                unnest(tokenizer);
                tokenizer->wordStatus = WORDSTATUS_OPERATOR;
                goto appendAndNext;
            }

            if (isShellOrCommandSubstitution(tokenizer)
                    && canBeginOperator(c)) {
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
                        tokenizer->tokenStatus == TOKEN_SUBSHELL) {
                    enum TokenType type = (tokenizer->wordStatus ==
                            WORDSTATUS_NUMBER) ? IO_NUMBER : TOKEN;
                    if (!delimit(tokenizer, type)) return TOKENIZER_ERROR;
                }

                tokenizer->wordStatus = WORDSTATUS_OPERATOR;
                goto appendAndNext;
            }

            if (isShellOrCommandSubstitution(tokenizer) && isblank(c)) {
                tokenizer->wordStatus = WORDSTATUS_NONE;
                if (tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
                        tokenizer->tokenStatus == TOKEN_SUBSHELL) {
                    if (!delimit(tokenizer, TOKEN)) return TOKENIZER_ERROR;
                    continue;
                }
                goto appendAndNext;
            }
        }

        if (isShellOrCommandSubstitution(tokenizer) &&
                tokenizer->wordStatus == WORDSTATUS_NONE && c == '#') {
            if (!nest(tokenizer, TOKEN_COMMENT)) return TOKENIZER_ERROR;
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
        if (!appendToStringBuffer(&tokenizer->buffer, c)) {
            return TOKENIZER_ERROR;
        }
    }

    if (tokenizer->tokenStatus != TOKEN_TOPLEVEL) {
        return TOKENIZER_NEED_INPUT;
    }

    if (tokenizer->wordStatus == WORDSTATUS_OPERATOR &&
            tokenizer->buffer.used == 1 &&
            tokenizer->buffer.buffer[0] == '\n') {
        if (!delimit(tokenizer, OPERATOR)) return TOKENIZER_ERROR;
        return TOKENIZER_DONE;
    }

    return TOKENIZER_NEED_INPUT;
}

void freeTokenizer(struct Tokenizer* tokenizer) {
    for (size_t i = 0; i < tokenizer->numTokens; i++) {
        free(tokenizer->tokens[i].text);
    }
    free(tokenizer->buffer.buffer);
}

static bool addToken(struct Tokenizer* tokenizer, enum TokenType type,
        char* text) {
    struct Token value = { .type = type, .text = text };
    return addToArray((void**) &tokenizer->tokens, &tokenizer->numTokens,
            &value, sizeof(struct Token));
}

static bool canBeginOperator(char c) {
    return c == '\n' || c == '&' || /*c == '(' ||*/ c == ')' || c == ';'
            || c == '<' || c == '>' || c == '|';
}

static bool canContinueOperator(const char* s, size_t opLength, char c) {
    if (opLength != 1) {
        return opLength == 2 && s[0] == '<' && s[1] == '<' && c == '-';
    }

    if (s[0] == '&') {
        return c == '&';
    } else if (s[0] == ';') {
        return c == ';';
    } else if (s[0] == '<') {
        return c == '<' || c == '&' || c == '>';
    } else if (s[0] == '>') {
        return c == '>' || c == '&' || c == '|';
    } else if (s[0] == '|') {
        return c == '|';
    }

    return false;
}

static bool delimit(struct Tokenizer* tokenizer, enum TokenType type) {
    assert(tokenizer->tokenStatus == TOKEN_TOPLEVEL ||
            tokenizer->tokenStatus == TOKEN_SUBSHELL);

    if (tokenizer->buffer.used == 0) return true;

    if (!addToken(tokenizer, type, finishStringBuffer(&tokenizer->buffer))) {
        return false;
    }

    if (!initStringBuffer(&tokenizer->buffer)) return false;

    return true;
}

static bool nest(struct Tokenizer* tokenizer, enum TokenStatus status) {
    tokenizer->wordStatus = WORDSTATUS_NONE;
    struct TokenizerContext* prev = malloc(sizeof(struct TokenizerContext));
    if (!prev) return false;
    prev->prev = tokenizer->prev;
    prev->tokenStatus = tokenizer->tokenStatus;
    tokenizer->prev = prev;
    tokenizer->tokenStatus = status;
    return true;
}

static void unnest(struct Tokenizer* tokenizer) {
    assert(tokenizer->prev);
    tokenizer->wordStatus = WORDSTATUS_WORD;
    struct TokenizerContext* prev = tokenizer->prev;
    tokenizer->tokenStatus = prev->tokenStatus;
    tokenizer->prev = prev->prev;
    free(prev);
}
