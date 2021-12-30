/* Copyright (c) 2021 Dennis Wölfing
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

/* sh/match.c
 * Pattern matching.
 */

#include <err.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include "expand.h"
#include "match.h"
#include "stringbuffer.h"

static bool isSpecialCharInBracketExpressions(char c) {
    return c == '[' || c == ']' || c == '!' || c == '^' || c == '-';
}

static bool isSpecialInDoubleQuotes(char c) {
    return c == '$' || c == '`' || c == '\\' || c == '"';
}

struct context {
    const char* pattern;
    size_t fieldIndex;
    size_t i;
    struct SubstitutionInfo* subst;
    size_t numSubstitutions;
    size_t substIndex;
    bool singleQuoted;
    bool doubleQuoted;
    bool escaped;
};

static bool nextChar(struct context* ctx, char* character) {
    struct SubstitutionInfo* subst = ctx->subst;
    while (ctx->pattern[ctx->i]) {
        while (subst && !(ctx->fieldIndex < subst->endField ||
                (ctx->fieldIndex == subst->endField && ctx->i < subst->end))) {
            ctx->substIndex++;
            if (ctx->substIndex < ctx->numSubstitutions) {
                subst = &ctx->subst[ctx->substIndex];
            } else {
                subst = NULL;
            }
        }

        bool inSubstitution = subst && (ctx->fieldIndex > subst->startField ||
                (ctx->fieldIndex == subst->startField &&
                ctx->i >= subst->begin));

        if (ctx->pattern[ctx->i] == '\'' && !ctx->doubleQuoted &&
                !ctx->escaped && !inSubstitution) {
            ctx->singleQuoted = !ctx->singleQuoted;
        } else if (ctx->pattern[ctx->i] == '"' && !ctx->singleQuoted &&
                !ctx->escaped && !inSubstitution) {
            ctx->doubleQuoted = !ctx->doubleQuoted;
        } else if (ctx->pattern[ctx->i] == '\\' && !ctx->singleQuoted &&
                !ctx->escaped && !inSubstitution && (!ctx->doubleQuoted ||
                isSpecialInDoubleQuotes(ctx->pattern[ctx->i + 1]))) {
            ctx->escaped = true;
        } else {
            bool literal = ctx->singleQuoted || ctx->doubleQuoted ||
                    ctx->escaped;
            *character = ctx->pattern[ctx->i];
            ctx->escaped = false;
            ctx->i++;
            return literal;
        }

        ctx->i++;
    }

    *character = '\0';
    return false;
}

static size_t prepareBracketExpression(const char* pattern, size_t fieldIndex,
        struct SubstitutionInfo* subst, size_t numSubstitutions,
        size_t expressionBegin, struct StringBuffer* buffer, bool pathname) {
    struct context context;
    context.pattern = pattern;
    context.fieldIndex = fieldIndex;
    context.subst = subst;
    context.numSubstitutions = numSubstitutions;
    context.substIndex = 0;
    context.singleQuoted = false;
    context.doubleQuoted = false;
    context.escaped = false;

    struct StringBuffer expr;
    initStringBuffer(&expr);
    appendToStringBuffer(&expr, '[');

    context.i = expressionBegin + 1;
    char c;
    bool literal = nextChar(&context, &c);
    while (c != '\0') {
        if (pathname && c == '/') {
            free(finishStringBuffer(&expr));
            return 0;
        } else if (literal && isSpecialCharInBracketExpressions(c)) {
            // We use collating symbols to force a character to be taken
            // literally.
            appendStringToStringBuffer(&expr, "[.");
            appendToStringBuffer(&expr, c);
            appendStringToStringBuffer(&expr, ".]");
        } else if (c == ']') {
            break;
        } else if (c == '[') {
            literal = nextChar(&context, &c);
            if (c == '.' || c == '=' || c == ':') {
                if (literal) {
                    appendStringToStringBuffer(&expr, "[.[.]");
                    continue;
                } else {
                    char end = c;
                    appendToStringBuffer(&expr, '[');
                    appendToStringBuffer(&expr, c);

                    literal = nextChar(&context, &c);
                    while (true) {
                        if (!literal && c == end) {
                            literal = nextChar(&context, &c);
                            if (!literal && c == ']') {
                                appendToStringBuffer(&expr, end);
                                appendToStringBuffer(&expr, ']');
                                break;
                            } else if (c == ']') {
                                appendToStringBuffer(&expr, end);
                                appendToStringBuffer(&expr, '\\');
                            } else {
                                appendToStringBuffer(&expr, end);
                            }
                        } else if (c == end) {
                            appendToStringBuffer(&expr, end);
                            literal = nextChar(&context, &c);
                            if (c == ']') {
                                appendToStringBuffer(&expr, '\\');
                            }
                        } else {
                            appendToStringBuffer(&expr, c);
                            literal = nextChar(&context, &c);
                        }
                    }
                }
            } else {
                appendToStringBuffer(&expr, '[');
                continue;
            }
        } else {
            appendToStringBuffer(&expr, c);
        }

        literal = nextChar(&context, &c);
    }

    if (c != ']') {
        free(finishStringBuffer(&expr));
        return 0;
    }
    appendToStringBuffer(&expr, ']');
    char* str = finishStringBuffer(&expr);
    appendStringToStringBuffer(buffer, str);
    free(str);
    return context.i - expressionBegin;
}

static char* preparePattern(const char* pattern, size_t fieldIndex,
        struct SubstitutionInfo* subst, size_t numSubstitutions, bool pathname,
        bool* containsSpecial) {
    struct StringBuffer buffer;
    initStringBuffer(&buffer);

    struct context context;
    context.pattern = pattern;
    context.fieldIndex = fieldIndex;
    context.i = 0;
    context.subst = subst;
    context.numSubstitutions = numSubstitutions;
    context.substIndex = 0;
    context.singleQuoted = false;
    context.doubleQuoted = false;
    context.escaped = false;
    *containsSpecial = false;

    char c;
    bool literal = nextChar(&context, &c);

    while (c != '\0') {
        if (literal && (c == '\\' || c == '?' || c == '*' || c == '[')) {
            appendToStringBuffer(&buffer, '\\');
            appendToStringBuffer(&buffer, c);
        } else if (c == '[') {
            size_t length = prepareBracketExpression(pattern, fieldIndex, subst,
                    numSubstitutions, context.i - 1, &buffer, pathname);
            if (length == 0) {
                // Not a valid bracket expression
                appendToStringBuffer(&buffer, '\\');
                appendToStringBuffer(&buffer, '[');
            } else {
                context.i += length - 1;
                *containsSpecial = true;
            }
        } else {
            appendToStringBuffer(&buffer, c);
            if (c == '?' || c == '*') {
                *containsSpecial = true;
            }
        }

        literal = nextChar(&context, &c);
    }

    return finishStringBuffer(&buffer);
}

bool matchesPattern(const char* expandedWord, const char* pattern) {
    struct ExpandContext context;
    char** fields;
    ssize_t numFields = expand2(pattern, EXPAND_NO_FIELD_SPLIT, &fields,
            &context);
    if (numFields < 0) return false;

    bool containsSpecial;
    char* prepared = preparePattern(fields[0], 0, context.substitutions,
            context.numSubstitutions, false, &containsSpecial);

    bool result = fnmatch(prepared, expandedWord, 0) == 0;
    free(prepared);
    free(context.substitutions);
    free(context.temp);
    free(fields);
    return result;
}

bool expandPathnames(char** fields, size_t numFields, char*** pathnames,
        size_t* numPathnames, struct SubstitutionInfo* subst,
        size_t numSubstitutions) {
    for (size_t i = 0; i < numFields; i++) {
        bool containsSpecial;
        char* pattern = preparePattern(fields[i], i, subst, numSubstitutions,
                true, &containsSpecial);
        if (containsSpecial) {
            glob_t data;
            int result = glob(pattern, 0, NULL, &data);

            if (result == 0) {
                for (size_t j = 0; j < data.gl_pathc; j++) {
                    char* str = strdup(data.gl_pathv[j]);
                    if (!str) err(1, "malloc");
                    addToArray((void**) pathnames, numPathnames, &str,
                            sizeof(char*));
                }
            } else if (result == GLOB_NOMATCH) {
                char* str = removeQuotes(fields[i], i, subst, numSubstitutions);
                addToArray((void**) pathnames, numPathnames, &str,
                        sizeof(char*));
            } else {
                warnx("pathname expansion error");
                globfree(&data);
                return false;
            }
            globfree(&data);
        } else {
            char* str = removeQuotes(fields[i], i, subst, numSubstitutions);
            addToArray((void**) pathnames, numPathnames, &str, sizeof(char*));
        }
        free(pattern);
    }

    return true;
}
