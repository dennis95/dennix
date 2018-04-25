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

/* sh/parser.c
 * Shell parser.
 */

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "tokenizer.h"
#include "parser.h"

static bool parseIoRedirect(struct Parser* parser, int fd,
        struct Redirection* result);
static void syntaxError(const char* token);

void initParser(struct Parser* parser, const struct Tokenizer* tokenizer) {
    parser->tokenizer = tokenizer;
    parser->offset = 0;
}

bool parseSimpleCommand(struct Parser* parser, struct SimpleCommand* command) {
    command->redirections = NULL;
    command->numRedirections = 0;
    command->words = NULL;
    command->numWords = 0;

    struct Token* tokens = parser->tokenizer->tokens;

    while (tokens[parser->offset].text[0] != '\n') {
        struct Token token = tokens[parser->offset];

        if (token.type == IO_NUMBER) {
            assert(parser->tokenizer->numTokens > parser->offset + 1 &&
                    tokens[parser->offset + 1].type == OPERATOR);

            int fd = strtol(token.text, NULL, 10);
            parser->offset++;

            struct Redirection redirection;
            if (!parseIoRedirect(parser, fd, &redirection)) return false;
            if (!addToArray((void**) &command->redirections,
                    &command->numRedirections, &redirection,
                    sizeof(redirection))) {
                return false;
            }
        } else if (token.type == OPERATOR) {
            struct Redirection redirection;
            if (!parseIoRedirect(parser, -1, &redirection)) return false;
            if (!addToArray((void**) &command->redirections,
                    &command->numRedirections, &redirection,
                    sizeof(redirection))) {
                return false;
            }
        } else {
            assert(token.type == TOKEN);
            if (!addToArray((void**) &command->words, &command->numWords,
                    &token.text, sizeof(char*))) {
                return false;
            }
            parser->offset++;
        }
    }

    return true;
}

void freeSimpleCommand(struct SimpleCommand* command) {
    free(command->redirections);
    free(command->words);
}

static bool parseIoRedirect(struct Parser* parser, int fd,
        struct Redirection* result) {
    const char* operator = parser->tokenizer->tokens[parser->offset].text;

    result->filenameIsFd = false;
    result->flags = 0;

    if (strcmp(operator, "<") == 0) {
        result->flags = O_RDONLY;
    } else if (strcmp(operator, ">") == 0) {
        result->flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(operator, ">|") == 0) {
        result->flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(operator, ">>") == 0) {
        result->flags = O_WRONLY | O_APPEND | O_CREAT;
    } else if (strcmp(operator, "<&") == 0) {
        result->filenameIsFd = true;
    } else if (strcmp(operator, ">&") == 0) {
        result->filenameIsFd = true;
    } else if (strcmp(operator, "<>") == 0) {
        result->flags = O_RDWR | O_CREAT;
    } else {
        syntaxError(operator);
        return false;
    }

    if (fd == -1) {
        fd = operator[0] == '<' ? 0 : 1;
    }
    result->fd = fd;

    parser->offset++;
    struct Token filename = parser->tokenizer->tokens[parser->offset];
    if (filename.type != TOKEN) {
        syntaxError(filename.text);
        return false;
    }
    result->filename = filename.text;

    parser->offset++;
    return true;
}

static void syntaxError(const char* token) {
    if (strcmp(token, "\n") == 0) {
        warnx("syntax error: unexpected newline");
    } else {
        warnx("syntax error: unexpected '%s'", token);
    }
}
