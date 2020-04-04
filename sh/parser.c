/* Copyright (c) 2018, 2019, 2020 Dennis Wölfing
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
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

#define BACKTRACKING // specify that a function might return PARSER_BACKTRACK.

static BACKTRACKING enum ParserResult parseIoRedirect(struct Parser* parser,
        int fd, struct Redirection* result);
static enum ParserResult parseLinebreak(struct Parser* parser);
static enum ParserResult parseList(struct Parser* parser, struct List* list);
static enum ParserResult parsePipeline(struct Parser* parser,
        struct Pipeline* pipeline);
static enum ParserResult parseSimpleCommand(struct Parser* parser,
        struct SimpleCommand* command);
static void syntaxError(struct Token* token);

static void freeList(struct List* list);
static void freePipeline(struct Pipeline* pipeline);
static void freeSimpleCommand(struct SimpleCommand* command);

static inline struct Token* getToken(struct Parser* parser) {
    if (parser->offset >= parser->tokenizer.numTokens) {
        return NULL;
    }
    return &parser->tokenizer.tokens[parser->offset];
}

void freeParser(struct Parser* parser) {
    freeTokenizer(&parser->tokenizer);
}

bool initParser(struct Parser* parser) {
    parser->offset = 0;
    return initTokenizer(&parser->tokenizer);
}

static enum ParserResult getNextLine(struct Parser* parser, bool newCommand) {
    enum TokenizerResult tokenResult;
    char* str;
    ssize_t length = readCommand(&str, newCommand);

continue_tokenizing:
    tokenResult = splitTokens(&parser->tokenizer, length < 0 ? "" : str);
    if (tokenResult == TOKENIZER_ERROR) {
        return PARSER_ERROR;
    } else if (tokenResult == TOKENIZER_PREMATURE_EOF) {
        syntaxError(NULL);
        return PARSER_SYNTAX;
    } else if (tokenResult == TOKENIZER_NEED_INPUT) {
        length = readCommand(&str, false);
        goto continue_tokenizing;
    }

    return PARSER_MATCH;
}

enum ParserResult parse(struct Parser* parser,
        struct CompleteCommand* command) {
    enum ParserResult result = getNextLine(parser, true);
    if (result != PARSER_MATCH) return result;

    if (parser->tokenizer.numTokens == 1 &&
            parser->tokenizer.tokens[0].type == OPERATOR &&
            parser->tokenizer.tokens[0].text[0] == '\n') {
        return PARSER_NO_CMD;
    }

    result = parseList(parser, &command->list);
    assert(result != PARSER_BACKTRACK);

    if (result == PARSER_MATCH &&
            parser->offset < parser->tokenizer.numTokens - 1) {
        syntaxError(getToken(parser));
        freeList(&command->list);
        return PARSER_SYNTAX;
    }

    if (result == PARSER_SYNTAX) {
        syntaxError(getToken(parser));
    }
    return result;
}

static enum ParserResult parseList(struct Parser* parser, struct List* list) {
    list->numPipelines = 0;
    list->pipelines = NULL;
    list->separators = NULL;

    enum ParserResult result;
    while (true) {
        struct Pipeline pipeline;
        result = parsePipeline(parser, &pipeline);
        if (result != PARSER_MATCH) goto fail;

        if (!addToArray((void**) &list->pipelines, &list->numPipelines,
                &pipeline, sizeof(pipeline))) {
            result = PARSER_ERROR;
            goto fail;
        }
        void* newSeperators = realloc(list->separators, list->numPipelines);
        if (!newSeperators) {
            result = PARSER_ERROR;
            goto fail;
        }
        list->separators = newSeperators;
        list->separators[list->numPipelines - 1] = LIST_SEMI;

        struct Token* token = getToken(parser);
        if (!token || token->type != OPERATOR) return PARSER_MATCH;

        if (strcmp(token->text, "&&") == 0 || strcmp(token->text, "||") == 0) {
            list->separators[list->numPipelines - 1] =
                    *token->text == '&' ? LIST_AND : LIST_OR;

            parser->offset++;
            result = parseLinebreak(parser);
            if (result != PARSER_MATCH) goto fail;
        } else if (strcmp(token->text, ";") == 0) {
            parser->offset++;
        } else {
            // TODO: Implement asynchronous lists.
            return PARSER_MATCH;
        }

        token = getToken(parser);

        if (list->separators[list->numPipelines - 1] == LIST_SEMI && (!token ||
                (token->type == OPERATOR && strcmp(token->text, "\n") == 0))) {
            return PARSER_MATCH;
        }
    }

fail:
    freeList(list);
    return result;
}

static enum ParserResult parsePipeline(struct Parser* parser,
        struct Pipeline* pipeline) {
    pipeline->commands = NULL;
    pipeline->numCommands = 0;
    pipeline->bang = false;

    enum ParserResult result;

    struct Token* token = getToken(parser);
    if (!token) return PARSER_SYNTAX;

    while (token->type == TOKEN && strcmp(token->text, "!") == 0) {
        pipeline->bang = !pipeline->bang;
        parser->offset++;
        token = getToken(parser);
        if (!token) return PARSER_SYNTAX;
    }

    while (true) {
        struct SimpleCommand command;
        result = parseSimpleCommand(parser, &command);
        if (result != PARSER_MATCH) goto fail;

        if (!addToArray((void**) &pipeline->commands, &pipeline->numCommands,
                &command, sizeof(command))) {
            result = PARSER_ERROR;
            goto fail;
        }

        token = getToken(parser);
        if (!token) return PARSER_MATCH;
        if (token->type != OPERATOR || strcmp(token->text, "|") != 0) {
            return PARSER_MATCH;
        }

        parser->offset++;

        result = parseLinebreak(parser);
        if (result != PARSER_MATCH) goto fail;
        token = getToken(parser);
    }

    return PARSER_MATCH;

fail:
    freePipeline(pipeline);
    return result;
}

static bool isName(const char* s, size_t length) {
    if (!isalpha(s[0]) && s[0] != '_') return false;
    for (size_t i = 1; i < length; i++) {
        if (!isalnum(s[i]) && s[i] != '_') return false;
    }
    return true;
}

static enum ParserResult parseSimpleCommand(struct Parser* parser,
        struct SimpleCommand* command) {
    command->assignmentWords = NULL;
    command->numAssignmentWords = 0;
    command->redirections = NULL;
    command->numRedirections = 0;
    command->words = NULL;
    command->numWords = 0;

    enum ParserResult result;
    bool hadNonAssignmentWord = false;

    struct Token* token = getToken(parser);
    assert(token);

    while (true) {
        if (token->type == IO_NUMBER || token->type == OPERATOR) {
            int fd = -1;
            if (token->type == IO_NUMBER) {
                fd = strtol(token->text, NULL, 10);
                parser->offset++;
            }

            struct Redirection redirection;
            result = parseIoRedirect(parser, fd, &redirection);

            if (result == PARSER_BACKTRACK) {
                if (command->numWords > 0 || command->numRedirections > 0 ||
                        command->numAssignmentWords > 0) {
                    return PARSER_MATCH;
                }
                result = PARSER_SYNTAX;
                goto fail;
            }

            if (result != PARSER_MATCH) goto fail;

            if (!addToArray((void**) &command->redirections,
                    &command->numRedirections, &redirection,
                    sizeof(redirection))) {
                result = PARSER_ERROR;
                goto fail;
            }
        } else {
            assert(token->type == TOKEN);

            const char* equals = strchr(token->text, '=');
            if (!hadNonAssignmentWord && equals && equals != token->text &&
                    isName(token->text, equals - token->text)) {
                if (!addToArray((void**) &command->assignmentWords,
                        &command->numAssignmentWords, &token->text,
                        sizeof(char*))) {
                    result = PARSER_ERROR;
                    goto fail;
                }
            } else {
                hadNonAssignmentWord = true;
                if (!addToArray((void**) &command->words, &command->numWords,
                        &token->text, sizeof(char*))) {
                    result = PARSER_ERROR;
                    goto fail;
                }
            }
            parser->offset++;
        }

        token = getToken(parser);
        if (!token) return PARSER_MATCH;
    }

    return PARSER_MATCH;

fail:
    freeSimpleCommand(command);
    return result;
}

static BACKTRACKING enum ParserResult parseIoRedirect(struct Parser* parser,
        int fd, struct Redirection* result) {
    struct Token* token = getToken(parser);
    assert(token && token->type == OPERATOR);
    const char* operator = token->text;

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
        return PARSER_BACKTRACK;
    }

    if (fd == -1) {
        fd = operator[0] == '<' ? 0 : 1;
    }
    result->fd = fd;

    parser->offset++;
    struct Token* filename = getToken(parser);
    if (!filename || filename->type != TOKEN) {
        return PARSER_SYNTAX;
    }
    result->filename = filename->text;

    parser->offset++;
    return PARSER_MATCH;
}

static enum ParserResult parseLinebreak(struct Parser* parser) {
    struct Token* token = getToken(parser);
    if (!token) {
        syntaxError(NULL);
        return PARSER_SYNTAX;
    }

    while (token->type == OPERATOR && strcmp(token->text, "\n") == 0) {
        parser->offset++;
        token = getToken(parser);
        if (!token) {
            enum ParserResult result = getNextLine(parser, false);
            if (result != PARSER_MATCH) return result;
            token = getToken(parser);
            if (!token) {
                syntaxError(NULL);
                return PARSER_SYNTAX;
            }
        }
    }
    return PARSER_MATCH;
}

static void syntaxError(struct Token* token) {
    if (!token) {
        warnx("syntax error: unexpected end of file");
    } else if (strcmp(token->text, "\n") == 0) {
        warnx("syntax error: unexpected newline");
    } else {
        warnx("syntax error: unexpected '%s'", token->text);
    }
}

void freeCompleteCommand(struct CompleteCommand* command) {
    freeList(&command->list);
}

static void freeList(struct List* list) {
    for (size_t i = 0; i < list->numPipelines; i++) {
        freePipeline(&list->pipelines[i]);
    }
    free(list->pipelines);
    free(list->separators);
}

static void freePipeline(struct Pipeline* pipeline) {
    for (size_t i = 0; i < pipeline->numCommands; i++) {
        freeSimpleCommand(&pipeline->commands[i]);
    }
    free(pipeline->commands);
}

static void freeSimpleCommand(struct SimpleCommand* command) {
    free(command->assignmentWords);
    free(command->redirections);
    free(command->words);
}
