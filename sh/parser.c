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

#include "expand.h"
#include "parser.h"
#include "stringbuffer.h"

#define BACKTRACKING // specify that a function might return PARSER_BACKTRACK.

static enum ParserResult parseCommand(struct Parser* parser,
        struct Command* command);
static enum ParserResult parseCompoundListWithTerminator(struct Parser* parser,
        struct List* list, const char* terminator);
static enum ParserResult parseForClause(struct Parser* parser,
        struct ForClause* clause);
static enum ParserResult parseCaseClause(struct Parser* parser,
        struct CaseClause* clause);
static enum ParserResult parseIfClause(struct Parser* parser,
        struct IfClause* clause);
static BACKTRACKING enum ParserResult parseIoRedirect(struct Parser* parser,
        struct Redirection* result);
static enum ParserResult parseLinebreak(struct Parser* parser);
static enum ParserResult parseList(struct Parser* parser, struct List* list,
        bool compound, bool allowLinebreak);
static enum ParserResult parsePipeline(struct Parser* parser,
        struct Pipeline* pipeline);
static enum ParserResult parseSimpleCommand(struct Parser* parser,
        struct SimpleCommand* command);
static void syntaxError(struct Token* token);

static void freeCommand(struct Command* command);
static void freeList(struct List* list);
static void freePipeline(struct Pipeline* pipeline);
static void freeSimpleCommand(struct SimpleCommand* command);

static inline struct Token* getToken(struct Parser* parser) {
    if (parser->offset >= parser->tokenizer.numTokens) {
        if (!*parser->tokenizer.input) return NULL;

        enum TokenizerResult tokenResult = splitTokens(&parser->tokenizer);
        if (tokenResult == TOKENIZER_PREMATURE_EOF) {
            syntaxError(NULL);
            return NULL;
        } else if (tokenResult == TOKENIZER_SYNTAX_ERROR) {
            return NULL;
        }
    }
    return &parser->tokenizer.tokens[parser->offset];
}

void freeParser(struct Parser* parser) {
    freeTokenizer(&parser->tokenizer);
}

void initParser(struct Parser* parser,
        void (*readCommand)(const char** str, bool newCommand, void* context),
        void* context) {
    parser->offset = 0;
    parser->hereDocOffset = 0;
    initTokenizer(&parser->tokenizer, readCommand, context);
}

static bool isReservedWord(const char* word) {
    static const char* reservedWords[]= { "!", "{", "}", "case", "do", "done",
            "elif", "else", "esac", "fi", "for", "if", "in", "then", "until",
            "while", NULL };
    size_t i = 0;
    while (reservedWords[i]) {
        if (strcmp(word, reservedWords[i]) == 0) return true;
        i++;
    }
    return false;
}

static bool isCompoundListTerminator(const char* word) {
    static const char* terminators[]= { ")", "}", ";;", "do", "done", "elif",
            "else", "esac", "fi", "then", NULL };
    size_t i = 0;
    while (terminators[i]) {
        if (strcmp(word, terminators[i]) == 0) return true;
        i++;
    }
    return false;
}

enum ParserResult parse(struct Parser* parser,
        struct CompleteCommand* command, bool readWholeScript) {
    splitTokens(&parser->tokenizer);
    struct Token* token = getToken(parser);
    if (readWholeScript) {
        if (!token) return PARSER_NO_CMD;
        enum ParserResult result = parseLinebreak(parser);
        if (result != PARSER_MATCH) return result;
        if (!getToken(parser)) return PARSER_NO_CMD;
    } else {
        if (!token || (token->type == OPERATOR && token->text[0] == '\n')) {
            return PARSER_NO_CMD;
        }
    }

    enum ParserResult result = parseList(parser, &command->list, false,
            readWholeScript);
    assert(result != PARSER_BACKTRACK);

    if (result == PARSER_MATCH && (*parser->tokenizer.input ||
            parser->tokenizer.wordStatus != WORDSTATUS_NONE ||
            parser->offset < parser->tokenizer.numTokens - 1)) {
        syntaxError(getToken(parser));
        freeList(&command->list);
        return PARSER_SYNTAX;
    }

    if (result == PARSER_SYNTAX) {
        syntaxError(getToken(parser));
    }
    return result;
}

enum ParserResult parseCommandSubstitution(struct Parser* parser,
        struct CompleteCommand* command, size_t* inputRemaining) {
    splitTokens(&parser->tokenizer);

    enum ParserResult result = parseLinebreak(parser);
    if (result != PARSER_MATCH) return result;

    struct Token* token = getToken(parser);
    if (!token) return PARSER_SYNTAX;
    if (token->type == OPERATOR && strcmp(token->text, ")") == 0) {
        *inputRemaining = strlen(parser->tokenizer.input);
        return PARSER_NO_CMD;
    }

    struct CompleteCommand dummy;
    struct CompleteCommand* parsedCommand = command ? command : &dummy;
    result = parseList(parser, &parsedCommand->list, true, true);
    if (result == PARSER_MATCH) {
        if (!command) {
            freeCompleteCommand(&dummy);
        }

        token = getToken(parser);
        if (!token || token->type != OPERATOR ||
                strcmp(token->text, ")") != 0) {
            if (command) {
                freeCompleteCommand(command);
            }
            return PARSER_SYNTAX;
        }

        *inputRemaining = strlen(parser->tokenizer.input);
    }
    return result;
}

static enum ParserResult parseList(struct Parser* parser, struct List* list,
        bool compound, bool allowLinebreak) {
    list->numPipelines = 0;
    list->pipelines = NULL;
    list->separators = NULL;

    enum ParserResult result;
    if (allowLinebreak) {
        result = parseLinebreak(parser);
        if (result != PARSER_MATCH) goto fail;
    }

    while (true) {
        struct Pipeline pipeline;
        result = parsePipeline(parser, &pipeline);
        if (result != PARSER_MATCH) goto fail;

        addToArray((void**) &list->pipelines, &list->numPipelines, &pipeline,
                sizeof(pipeline));
        list->separators = realloc(list->separators, list->numPipelines);
        if (!list->separators) err(1, "realloc");
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
            if (allowLinebreak) {
                result = parseLinebreak(parser);
                if (result != PARSER_MATCH) goto fail;
            }
        } else if (allowLinebreak && strcmp(token->text, "\n") == 0) {
            result = parseLinebreak(parser);
            if (result != PARSER_MATCH) goto fail;
        } else {
            // TODO: Implement asynchronous lists.
            return PARSER_MATCH;
        }

        token = getToken(parser);

        if (compound && list->separators[list->numPipelines - 1] == LIST_SEMI) {
            if (!token) {
                result = PARSER_SYNTAX;
                goto fail;
            }
            if (isCompoundListTerminator(token->text)) {
                return PARSER_MATCH;
            }
        } else if (list->separators[list->numPipelines - 1] == LIST_SEMI &&
                (!token || (token->type == OPERATOR &&
                strcmp(token->text, "\n") == 0))) {
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
        struct Command command;
        result = parseCommand(parser, &command);
        if (result != PARSER_MATCH) goto fail;

        addToArray((void**) &pipeline->commands, &pipeline->numCommands,
                &command, sizeof(command));

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

static enum ParserResult parseCompoundListWithTerminator(struct Parser* parser,
        struct List* list, const char* terminator) {
    enum ParserResult result = parseList(parser, list, true, true);
    if (result != PARSER_MATCH) return result;
    struct Token* token = getToken(parser);
    if (!token || strcmp(token->text, terminator) != 0) return PARSER_SYNTAX;
    parser->offset++;
    return PARSER_MATCH;
}

static enum ParserResult parseCommand(struct Parser* parser,
        struct Command* command) {
    command->redirections = NULL;
    command->numRedirections = 0;
    struct Token* token = getToken(parser);
    enum ParserResult result;
    if (!isReservedWord(token->text) && strcmp(token->text, "(") != 0) {
        command->type = COMMAND_SIMPLE;
        // Redirections are already handled by parseSimpleCommand.
        return parseSimpleCommand(parser, &command->simpleCommand);
    } else if (strcmp(token->text, "(") == 0) {
        command->type = COMMAND_SUBSHELL;
        parser->offset++;
        result = parseCompoundListWithTerminator(parser, &command->compoundList,
                ")");
    } else if (strcmp(token->text, "{") == 0) {
        command->type = COMMAND_BRACE_GROUP;
        parser->offset++;
        result = parseCompoundListWithTerminator(parser, &command->compoundList,
                "}");
    } else if (strcmp(token->text, "for") == 0) {
        command->type = COMMAND_FOR;
        result = parseForClause(parser, &command->forClause);
    } else if (strcmp(token->text, "case") == 0) {
        command->type = COMMAND_CASE;
        result = parseCaseClause(parser, &command->caseClause);
    } else if (strcmp(token->text, "if") == 0) {
        command->type = COMMAND_IF;
        result = parseIfClause(parser, &command->ifClause);
    } else if (strcmp(token->text, "while") == 0) {
        command->type = COMMAND_WHILE;
        parser->offset++;
        result = parseCompoundListWithTerminator(parser,
                &command->loop.condition, "do");
        if (result != PARSER_MATCH) return result;
        result = parseCompoundListWithTerminator(parser, &command->loop.body,
                "done");
        if (result != PARSER_MATCH) {
            freeList(&command->loop.condition);
        }
    } else if (strcmp(token->text, "until") == 0) {
        command->type = COMMAND_UNTIL;
        parser->offset++;
        result = parseCompoundListWithTerminator(parser,
                &command->loop.condition, "do");
        if (result != PARSER_MATCH) return result;
        result = parseCompoundListWithTerminator(parser, &command->loop.body,
                "done");
        if (result != PARSER_MATCH) {
            freeList(&command->loop.condition);
        }
    } else {
        return PARSER_SYNTAX;
    }

    if (result != PARSER_MATCH) {
        return result;
    }

    // Parse redirections.
    token = getToken(parser);
    if (!token) return PARSER_MATCH;

    while (token->type == IO_NUMBER || token->type == OPERATOR) {
        struct Redirection redirection;
        result = parseIoRedirect(parser, &redirection);

        if (result == PARSER_BACKTRACK) return PARSER_MATCH;
        if (result != PARSER_MATCH) {
            freeCommand(command);
            return result;
        }

        addToArray((void**) &command->redirections, &command->numRedirections,
                &redirection, sizeof(redirection));
        token = getToken(parser);
        if (!token) return PARSER_MATCH;
    }
    return PARSER_MATCH;
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
            struct Redirection redirection;
            result = parseIoRedirect(parser, &redirection);

            if (result == PARSER_BACKTRACK) {
                if (command->numWords > 0 || command->numRedirections > 0 ||
                        command->numAssignmentWords > 0) {
                    return PARSER_MATCH;
                }
                result = PARSER_SYNTAX;
                goto fail;
            }

            if (result != PARSER_MATCH) goto fail;

            addToArray((void**) &command->redirections,
                    &command->numRedirections, &redirection,
                    sizeof(redirection));
        } else {
            assert(token->type == TOKEN);

            const char* equals = strchr(token->text, '=');
            if (!hadNonAssignmentWord && equals && equals != token->text &&
                    isName(token->text, equals - token->text)) {
                addToArray((void**) &command->assignmentWords,
                        &command->numAssignmentWords, &token->text,
                        sizeof(char*));
            } else {
                hadNonAssignmentWord = true;
                addToArray((void**) &command->words, &command->numWords,
                        &token->text, sizeof(char*));
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
        struct Redirection* result) {
    struct Token* token = getToken(parser);
    assert(token);
    int fd = -1;
    if (token->type == IO_NUMBER) {
        fd = strtol(token->text, NULL, 10);
        parser->offset++;
        token = getToken(parser);
    }

    assert(token && token->type == OPERATOR);
    const char* operator = token->text;

    if (strcmp(operator, "<") == 0) {
        result->type = REDIR_INPUT;
    } else if (strcmp(operator, ">") == 0) {
        result->type = REDIR_OUTPUT;
    } else if (strcmp(operator, ">|") == 0) {
        result->type = REDIR_OUTPUT_CLOBBER;
    } else if (strcmp(operator, ">>") == 0) {
        result->type = REDIR_APPEND;
    } else if (strcmp(operator, "<&") == 0) {
        result->type = REDIR_DUP;
    } else if (strcmp(operator, ">&") == 0) {
        result->type = REDIR_DUP;
    } else if (strcmp(operator, "<>") == 0) {
        result->type = REDIR_READ_WRITE;
    } else if (strcmp(operator, "<<") == 0 || strcmp(operator, "<<-") == 0 ) {
        result->type = REDIR_HERE_DOC;
    } else {
        return PARSER_BACKTRACK;
    }

    if (fd == -1) {
        fd = operator[0] == '<' ? 0 : 1;
    }
    result->fd = fd;

    parser->offset++;
    struct Token* word = getToken(parser);
    if (!word || word->type != TOKEN) {
        return PARSER_SYNTAX;
    }
    result->filename = word->text;
    if (result->type == REDIR_HERE_DOC) {
        struct HereDoc* hereDoc =
                &parser->tokenizer.hereDocs[parser->hereDocOffset];
        if (strcmp(hereDoc->delimiter, word->text) != 0) {
            result->type = REDIR_HERE_DOC_QUOTED;
        }

        while (!hereDoc->content) {
            if (splitTokens(&parser->tokenizer) != TOKENIZER_DONE) {
                return PARSER_SYNTAX;
            }
            hereDoc = &parser->tokenizer.hereDocs[parser->hereDocOffset];
        }
        result->filename = hereDoc->content;
        parser->hereDocOffset++;
    }

    parser->offset++;
    return PARSER_MATCH;
}

static enum ParserResult parseForClause(struct Parser* parser,
        struct ForClause* clause) {
    clause->words = NULL;
    clause->numWords = 0;
    parser->offset++;
    struct Token* token = getToken(parser);
    if (!token || !isName(token->text, strlen(token->text))) {
        return PARSER_SYNTAX;
    }
    clause->name = token->text;
    parser->offset++;
    token = getToken(parser);
    if (!token) return PARSER_SYNTAX;
    if (strcmp(token->text, "in") == 0) {
        parser->offset++;
        token = getToken(parser);
        if (!token) return PARSER_SYNTAX;
        while (token->type == TOKEN) {
            addToArray((void**) &clause->words, &clause->numWords, &token->text,
                    sizeof(char*));
            parser->offset++;
            token = getToken(parser);
            if (!token) goto syntax;
        }
        if (strcmp(token->text, ";") == 0) {
            parser->offset++;
        } else if (strcmp(token->text, "\n") != 0) {
            goto syntax;
        }
    } else {
        const char* word = "\"$@\"";
        addToArray((void**) &clause->words, &clause->numWords, &word,
                sizeof(char*));
        if (strcmp(token->text, ";") == 0) {
            parser->offset++;
        }
    }
    enum ParserResult result = parseLinebreak(parser);
    if (result != PARSER_MATCH) goto syntax;

    token = getToken(parser);
    if (!token || strcmp(token->text, "do") != 0) goto syntax;
    parser->offset++;
    result = parseCompoundListWithTerminator(parser, &clause->body, "done");
    if (result == PARSER_MATCH) return PARSER_MATCH;

syntax:
    free(clause->words);
    return PARSER_SYNTAX;
}

static enum ParserResult parseCaseClause(struct Parser* parser,
        struct CaseClause* clause) {
    clause->items = NULL;
    clause->numItems = 0;
    parser->offset++;
    struct Token* token = getToken(parser);
    if (!token || token->type != TOKEN) {
        return PARSER_SYNTAX;
    }
    clause->word = token->text;
    parser->offset++;

    enum ParserResult result = parseLinebreak(parser);
    if (result != PARSER_MATCH) return result;

    token = getToken(parser);
    if (!token || strcmp(token->text, "in") != 0) {
        return PARSER_SYNTAX;
    }
    parser->offset++;

    result = parseLinebreak(parser);
    if (result != PARSER_MATCH) return result;

    token = getToken(parser);
    if (!token) return PARSER_SYNTAX;

    while (strcmp(token->text, "esac") != 0) {
        struct CaseItem item;
        item.patterns = NULL;
        item.numPatterns = 0;
        item.hasList = false;
        item.fallthrough = false;

        if (token->type != TOKEN) {
            if (strcmp(token->text, "(") != 0) goto syntax;
            parser->offset++;
            token = getToken(parser);
            if (!token) goto syntax;
        }

        while (true) {
            if (token->type != TOKEN) goto fail;
            addToArray((void**) &item.patterns, &item.numPatterns, &token->text,
                    sizeof(char*));
            parser->offset++;
            token = getToken(parser);
            if (!token) goto fail;

            if (token->type == OPERATOR && strcmp(token->text, "|") == 0) {
                parser->offset++;
                token = getToken(parser);
                if (!token) goto fail;
            } else {
                break;
            }
        }

        if (token->type != OPERATOR || strcmp(token->text, ")") != 0 ||
                item.numPatterns == 0) {
            goto fail;
        }
        parser->offset++;
        result = parseLinebreak(parser);
        if (result != PARSER_MATCH) goto fail;
        token = getToken(parser);
        if (!token) goto fail;

        if (strcmp(token->text, "esac") != 0 &&
                strcmp(token->text, ";;") != 0 &&
                strcmp(token->text, ";&") != 0) {
            result = parseList(parser, &item.list, true, true);
            if (result != PARSER_MATCH) goto fail;
            item.hasList = true;
        }

        token = getToken(parser);
        if (!token) goto fail;

        if (token->type == OPERATOR) {
            if (strcmp(token->text, ";;") == 0) {
                parser->offset++;
            } else if (strcmp(token->text, ";&") == 0) {
                item.fallthrough = true;
                parser->offset++;
            } else {
                goto fail;
            }

            result = parseLinebreak(parser);
            if (result != PARSER_MATCH) goto fail;
            token = getToken(parser);
            if (!token) goto fail;
        } else if (strcmp(token->text, "esac") != 0) {
            goto fail;
        }

        addToArray((void**) &clause->items, &clause->numItems, &item,
                sizeof(struct CaseItem));

        continue;
fail:
        free(item.patterns);
        if (item.hasList) {
            freeList(&item.list);
        }
        goto syntax;
    }
    parser->offset++;
    return PARSER_MATCH;

syntax:
    for (size_t i = 0; i < clause->numItems; i++) {
        free(clause->items[i].patterns);
        if (clause->items[i].hasList) {
            freeList(&clause->items[i].list);
        }
    }
    free(clause->items);
    return PARSER_SYNTAX;
}

static enum ParserResult parseIfClause(struct Parser* parser,
        struct IfClause* clause) {
    clause->numConditions = 0;
    clause->conditions = NULL;
    clause->bodies = NULL;

    struct Token* token;
    enum ParserResult result;
    do {
        parser->offset++;
        struct List condition;
        result = parseCompoundListWithTerminator(parser, &condition, "then");
        if (result != PARSER_MATCH) goto fail;
        struct List body;
        result = parseList(parser, &body, true, true);
        if (result != PARSER_MATCH) {
            freeList(&condition);
            goto fail;
        }
        token = getToken(parser);
        if (!token) {
            result = PARSER_SYNTAX;
            freeList(&condition);
            freeList(&body);
            goto fail;
        }

        struct List* newLists = reallocarray(clause->conditions,
                clause->numConditions + 1, sizeof(struct List));
        if (!newLists) err(1, "realloc");

        clause->conditions = newLists;
        clause->conditions[clause->numConditions] = condition;
        newLists = reallocarray(clause->bodies, clause->numConditions + 1,
                sizeof(struct List));
        if (!newLists) err(1, "realloc");

        clause->bodies = newLists;
        clause->bodies[clause->numConditions] = body;
        clause->numConditions++;
    } while (strcmp(token->text, "elif") == 0);

    if (strcmp(token->text, "else") == 0) {
        parser->offset++;
        clause->hasElse = true;
        struct List body;
        result = parseCompoundListWithTerminator(parser, &body, "fi");
        if (result != PARSER_MATCH) {
            goto fail;
        }
        struct List* newLists = reallocarray(clause->bodies,
                clause->numConditions + 1, sizeof(struct List));
        if (!newLists) err(1, "realloc");
        clause->bodies = newLists;
        clause->bodies[clause->numConditions] = body;
    } else {
        if (strcmp(token->text, "fi") != 0) {
            result = PARSER_SYNTAX;
            goto fail;
        }
        clause->hasElse = false;
        parser->offset++;
    }
    return PARSER_MATCH;

fail:
    for (size_t i = 0; i < clause->numConditions; i++) {
        freeList(&clause->conditions[i]);
        freeList(&clause->bodies[i]);
    }
    free(clause->conditions);
    free(clause->bodies);
    return result;
}

static enum ParserResult parseLinebreak(struct Parser* parser) {
    struct Token* token = getToken(parser);
    if (!token) return PARSER_SYNTAX;

    while (token && token->type == OPERATOR && strcmp(token->text, "\n") == 0) {
        parser->offset++;
        token = getToken(parser);
        if (!token) {
            enum TokenizerResult tokenResult = splitTokens(&parser->tokenizer);
            if (tokenResult == TOKENIZER_PREMATURE_EOF) {
                syntaxError(NULL);
                return PARSER_SYNTAX;
            } else if (tokenResult == TOKENIZER_SYNTAX_ERROR) {
                return PARSER_SYNTAX;
            }
            token = getToken(parser);
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
        freeCommand(&pipeline->commands[i]);
    }
    free(pipeline->commands);
}

static void freeCommand(struct Command* command) {
    switch (command->type) {
    case COMMAND_SIMPLE:
        freeSimpleCommand(&command->simpleCommand);
        break;
    case COMMAND_SUBSHELL:
    case COMMAND_BRACE_GROUP:
        freeList(&command->compoundList);
        break;
    case COMMAND_FOR:
        free(command->forClause.words);
        freeList(&command->forClause.body);
        break;
    case COMMAND_CASE:
        for (size_t i = 0; i < command->caseClause.numItems; i++) {
            free(command->caseClause.items[i].patterns);
            if (command->caseClause.items[i].hasList) {
                freeList(&command->caseClause.items[i].list);
            }
        }
        free(command->caseClause.items);
        break;
    case COMMAND_IF:
        for (size_t i = 0; i < command->ifClause.numConditions; i++) {
            freeList(&command->ifClause.conditions[i]);
            freeList(&command->ifClause.bodies[i]);
        }
        if (command->ifClause.hasElse) {
            freeList(&command->ifClause.bodies[
                    command->ifClause.numConditions]);
        }
        free(command->ifClause.conditions);
        free(command->ifClause.bodies);
        break;
    case COMMAND_WHILE:
    case COMMAND_UNTIL:
        freeList(&command->loop.condition);
        freeList(&command->loop.body);
        break;
    }
    free(command->redirections);
}

static void freeSimpleCommand(struct SimpleCommand* command) {
    free(command->assignmentWords);
    free(command->redirections);
    free(command->words);
}
