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

/* sh/parser.h
 * Shell parser.
 */

#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "tokenizer.h"

struct Redirection {
    int fd;
    const char* filename;
    bool filenameIsFd;
    int flags;
};

struct SimpleCommand {
    char** assignmentWords;
    size_t numAssignmentWords;
    struct Redirection* redirections;
    size_t numRedirections;
    char** words;
    size_t numWords;
};

struct List {
    struct Pipeline* pipelines;
    char* separators;
    size_t numPipelines;
};

enum {
    LIST_AND,
    LIST_OR,
    LIST_SEMI,
};

struct ForClause {
    char* name;
    char** words;
    size_t numWords;
    struct List body;
};

struct IfClause {
    struct List* conditions;
    struct List* bodies;
    size_t numConditions;
    bool hasElse;
};

struct Loop {
    struct List condition;
    struct List body;
};

enum CommandType {
    COMMAND_SIMPLE,
    COMMAND_SUBSHELL,
    COMMAND_BRACE_GROUP,
    COMMAND_FOR,
    COMMAND_IF,
    COMMAND_WHILE,
    COMMAND_UNTIL,
};

struct Command {
    union {
        struct SimpleCommand simpleCommand;
        struct List compoundList;
        struct ForClause forClause;
        struct IfClause ifClause;
        struct Loop loop;
    };
    enum CommandType type;
};

struct Pipeline {
    struct Command* commands;
    size_t numCommands;
    bool bang;
};

struct CompleteCommand {
    struct List list;
};

struct Parser {
    struct Tokenizer tokenizer;
    size_t offset;
};

enum ParserResult {
    PARSER_MATCH,
    PARSER_SYNTAX,
    PARSER_NO_CMD,
    // The following results are only used internally.
    PARSER_BACKTRACK,
};

void freeParser(struct Parser* parser);
void initParser(struct Parser* parser);
enum ParserResult parse(struct Parser* parser,
        struct CompleteCommand* command);
void freeCompleteCommand(struct CompleteCommand* command);

#endif
