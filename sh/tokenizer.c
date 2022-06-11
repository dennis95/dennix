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

/* sh/tokenizer.c
 * Token recognition.
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "expand.h"
#include "parser.h"
#include "tokenizer.h"

static bool canBeginOperator(char c);
static bool canContinueOperator(const char* s, size_t opLength, char c);
static void delimit(struct Tokenizer* tokenizer, enum TokenType type);
static void nest(struct Tokenizer* tokenizer, enum TokenStatus status);
static bool readHereDocument(struct Tokenizer* tokenizer);
static void unnest(struct Tokenizer* tokenizer);

void initTokenizer(struct Tokenizer* tokenizer,
        void (*readCommand)(const char** str, bool newCommand, void* context),
        void* context) {
    tokenizer->backslash = false;
    tokenizer->numTokens = 0;
    tokenizer->prev = NULL;
    tokenizer->tokens = NULL;
    tokenizer->numHereDocs = 0;
    tokenizer->hereDocs = NULL;
    tokenizer->tokenStatus = TOKEN_TOPLEVEL;
    tokenizer->wordStatus = WORDSTATUS_NONE;
    tokenizer->input = NULL;
    tokenizer->readCommand = readCommand;
    tokenizer->context = context;

    initStringBuffer(&tokenizer->buffer);
}

static void readCommand(const char** str, bool newCommand, void* context) {
    (void) newCommand;
    struct Tokenizer* tokenizer = context;

    if (!*tokenizer->input) {
        tokenizer->readCommand(&tokenizer->input, false, tokenizer->context);
    }
    *str = tokenizer->input;
    appendStringToStringBuffer(&tokenizer->buffer, tokenizer->input);
    tokenizer->input += strlen(tokenizer->input);
}

enum TokenizerResult splitTokens(struct Tokenizer* tokenizer) {
    if (!tokenizer->input) {
        tokenizer->readCommand(&tokenizer->input, true, tokenizer->context);
        if (!*tokenizer->input) {
            return TOKENIZER_DONE;
        }
    }

    while (true) {
        char c = *tokenizer->input;

        if (!c) {
            if (tokenizer->tokenStatus == TOKEN_TOPLEVEL &&
                    tokenizer->wordStatus == WORDSTATUS_OPERATOR) {
                delimit(tokenizer, OPERATOR);
                tokenizer->wordStatus = WORDSTATUS_NONE;

                if (tokenizer->numHereDocs > 0 && !tokenizer->hereDocs[
                        tokenizer->numHereDocs - 1].content) {
                    tokenizer->wordStatus = WORDSTATUS_HERE_DOC;
                }

                return TOKENIZER_DONE;
            }

            tokenizer->readCommand(&tokenizer->input, false,
                    tokenizer->context);
            if (!*tokenizer->input) {
                if (tokenizer->tokenStatus != TOKEN_TOPLEVEL ||
                        tokenizer->wordStatus != WORDSTATUS_NONE) {
                    return TOKENIZER_PREMATURE_EOF;
                }
                return TOKENIZER_DONE;
            }
            continue;
        }

        if (tokenizer->wordStatus == WORDSTATUS_HERE_DOC) {
            if (readHereDocument(tokenizer)) {
                if (tokenizer->hereDocs[tokenizer->numHereDocs - 1].content) {
                    tokenizer->wordStatus = WORDSTATUS_NONE;
                }
                return TOKENIZER_DONE;
            }

            continue;
        }

        if (tokenizer->tokenStatus == TOKEN_COMMENT) {
            if (c == '\n') {
                unnest(tokenizer);
                continue;
            }
            tokenizer->input++;
            continue;
        }

        bool escaped = tokenizer->backslash;
        tokenizer->backslash = false;

        if (escaped && c == '\n') {
            tokenizer->buffer.used--;
            tokenizer->input++;
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
                delimit(tokenizer, OPERATOR);
                tokenizer->wordStatus = WORDSTATUS_NONE;

                char* op = tokenizer->tokens[tokenizer->numTokens - 1].text;
                if (strcmp(op, "<<") == 0 || strcmp(op, "<<-") == 0) {
                    tokenizer->numHereDocs++;
                    tokenizer->hereDocs = reallocarray(tokenizer->hereDocs,
                            tokenizer->numHereDocs, sizeof(struct HereDoc));
                    if (!tokenizer->hereDocs) err(1, "malloc");

                    struct HereDoc* newHereDoc =
                            &tokenizer->hereDocs[tokenizer->numHereDocs - 1];
                    newHereDoc->content = NULL;
                    newHereDoc->delimiter = NULL;
                    newHereDoc->stripTabs = op[2] == '-';
                } else if (strcmp(op, "\n") == 0) {
                    if (tokenizer->numHereDocs > 0 && !tokenizer->hereDocs[
                            tokenizer->numHereDocs - 1].content) {
                        tokenizer->wordStatus = WORDSTATUS_HERE_DOC;
                    }
                }

                return TOKENIZER_DONE;
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
                    appendToStringBuffer(&tokenizer->buffer, c);
                    tokenizer->input++;
                    struct Parser parser;
                    initParser(&parser, readCommand, tokenizer);
                    size_t inputRemaining;
                    enum ParserResult result = parseCommandSubstitution(&parser,
                            NULL, &inputRemaining);
                    freeParser(&parser);
                    if (result != PARSER_MATCH && result != PARSER_NO_CMD) {
                        return TOKENIZER_SYNTAX_ERROR;
                    }
                    tokenizer->input -= inputRemaining;
                    tokenizer->buffer.used -= inputRemaining;
                    continue;
                } else {
                    tokenizer->wordStatus = WORDSTATUS_WORD;
                }
            }

            if (tokenizer->tokenStatus == TOKEN_PARAMETER_EXP && c == '}') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus != TOKEN_SINGLE_QUOTED &&
                    tokenizer->tokenStatus != TOKEN_BACKTICK && c == '`') {
                nest(tokenizer, TOKEN_BACKTICK);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_BACKTICK && c == '`') {
                unnest(tokenizer);
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_TOPLEVEL
                    && canBeginOperator(c)) {
                if (tokenizer->buffer.used > 0) {
                    enum TokenType type = (tokenizer->wordStatus ==
                            WORDSTATUS_NUMBER) ? IO_NUMBER : TOKEN;
                    delimit(tokenizer, type);
                    tokenizer->wordStatus = WORDSTATUS_NONE;
                    return TOKENIZER_DONE;
                }

                tokenizer->wordStatus = WORDSTATUS_OPERATOR;
                goto appendAndNext;
            }

            if (tokenizer->tokenStatus == TOKEN_TOPLEVEL && isblank(c)) {
                tokenizer->wordStatus = WORDSTATUS_NONE;
                tokenizer->input++;
                if (tokenizer->buffer.used > 0) {
                    delimit(tokenizer, TOKEN);
                    return TOKENIZER_DONE;
                }
                continue;
            }
        }

        if (tokenizer->tokenStatus == TOKEN_TOPLEVEL &&
                tokenizer->wordStatus == WORDSTATUS_NONE && c == '#') {
            nest(tokenizer, TOKEN_COMMENT);
            tokenizer->input++;
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
        tokenizer->input++;
    }
}

void freeTokenizer(struct Tokenizer* tokenizer) {
    while (tokenizer->prev) {
        unnest(tokenizer);
    }

    for (size_t i = 0; i < tokenizer->numTokens; i++) {
        free(tokenizer->tokens[i].text);
    }
    free(tokenizer->tokens);

    for (size_t i = 0; i < tokenizer->numHereDocs; i++) {
        free(tokenizer->hereDocs[i].content);
        free(tokenizer->hereDocs[i].delimiter);
    }
    free(tokenizer->hereDocs);
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

    if (tokenizer->numHereDocs > 0 &&
            !tokenizer->hereDocs[tokenizer->numHereDocs - 1].delimiter) {
        char* delimiter = removeQuotes(token.text, 0, NULL, 0);
        tokenizer->hereDocs[tokenizer->numHereDocs - 1].delimiter = delimiter;
    }
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

static bool readHereDocument(struct Tokenizer* tokenizer) {
    size_t i = 0;
    while (tokenizer->hereDocs[i].content) {
        i++;
    }

    const char* delimiter = tokenizer->hereDocs[i].delimiter;
    size_t delimLength = strlen(delimiter);
    bool stripTabs = tokenizer->hereDocs[i].stripTabs;

    while (*tokenizer->input) {
        while (stripTabs && *tokenizer->input == '\t') {
            tokenizer->input++;
        }

        size_t lineLength = strcspn(tokenizer->input, "\n");
        if (lineLength == delimLength && memcmp(tokenizer->input, delimiter,
                lineLength) == 0) {
            tokenizer->hereDocs[i].content =
                    finishStringBuffer(&tokenizer->buffer);
            initStringBuffer(&tokenizer->buffer);
            tokenizer->input += lineLength;
            if (*tokenizer->input) {
                tokenizer->input++;
            }
            return true;
        }

        appendBytesToStringBuffer(&tokenizer->buffer, tokenizer->input,
                lineLength);
        appendToStringBuffer(&tokenizer->buffer, '\n');

        tokenizer->input += lineLength;
        if (*tokenizer->input) {
            tokenizer->input++;
        }
    }

    return false;
}

static void unnest(struct Tokenizer* tokenizer) {
    assert(tokenizer->prev);
    tokenizer->wordStatus = WORDSTATUS_WORD;
    struct TokenizerContext* prev = tokenizer->prev;
    tokenizer->tokenStatus = prev->tokenStatus;
    tokenizer->prev = prev->prev;
    free(prev);
}
