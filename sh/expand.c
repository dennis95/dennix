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

/* sh/expand.c
 * Word expansion.
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "execute.h"
#include "expand.h"
#include "match.h"
#include "stringbuffer.h"
#include "variables.h"

static bool doCommandSubstitution(const char** word,
        struct StringBuffer* sb, struct ExpandContext* context,
        bool doubleQuoted, bool oldStyle);
static ssize_t doDollarSubstitutions(const char* word, bool doubleQuoted,
        struct StringBuffer* sb, struct ExpandContext* context);
static char* doSubstitutions(const char* word, struct ExpandContext* context);
static size_t splitFields(char* word, struct ExpandContext* context,
        char*** result);

char* expandWord(const char* word) {
    char** fields;
    ssize_t numFields = expand(word, EXPAND_NO_FIELD_SPLIT, &fields);
    if (numFields < 0) return NULL;
    assert(numFields == 1);
    char* result = fields[0];
    free(fields);
    return result;
}

ssize_t expand2(const char* word, int flags, char*** result,
        struct ExpandContext* context) {
    context->substitutions = NULL;
    context->numSubstitutions = 0;
    context->flags = flags;
    context->deleteIfEmpty = false;

    context->temp = doSubstitutions(word, context);
    if (!context->temp) {
        free(context->substitutions);
        return -1;
    }

    char** fields;
    ssize_t numFields;
    if (flags & EXPAND_NO_FIELD_SPLIT) {
        fields = malloc(sizeof(char*));
        if (!fields) err(1, "malloc");
        fields[0] = context->temp;
        numFields = 1;
    } else {
        numFields = splitFields(context->temp, context, &fields);
    }

    *result = fields;
    return numFields;
}

ssize_t expand(const char* word, int flags, char*** result) {
    struct ExpandContext context;
    char** fields;
    ssize_t numFields = expand2(word, flags, &fields, &context);
    if (numFields < 0) return -1;

    if (flags & EXPAND_PATHNAMES && !shellOptions.noglob) {
        char** newFields = NULL;
        size_t numNewFields = 0;

        if (!expandPathnames(fields, numFields, &newFields, &numNewFields,
                context.substitutions, context.numSubstitutions)) {
            free(context.substitutions);
            free(context.temp);
            return -1;
        }

        free(fields);
        fields = newFields;
        numFields = numNewFields;
    } else {
        for (ssize_t i = 0; i < numFields; i++) {
            fields[i] = removeQuotes(fields[i], i, context.substitutions,
                    context.numSubstitutions);
        }
    }

    if (context.deleteIfEmpty && numFields == 1 && *fields[0] == '\0' &&
            context.numSubstitutions == 0) {
        free(fields[0]);
        numFields = 0;
    }

    free(context.substitutions);
    free(context.temp);
    *result = fields;
    return numFields;
}

static bool isPositionalParameter(const char* s) {
    do {
        if (!isdigit(*s)) return false;
    } while (*++s);
    return true;
}

static bool isSpecialParameter(char c) {
    return strchr("!#$*-?@", c);
}

static void substitute(const char* value, struct StringBuffer* sb,
        struct ExpandContext* context, bool doubleQuoted, bool splitAtEnd) {
    if (!value) value = "";
    struct SubstitutionInfo info;
    info.begin = sb->used;
    info.end = info.begin + strlen(value);
    info.startField = 0;
    info.endField = 0;
    info.applyFieldSplitting = !doubleQuoted;
    info.splitAtEnd = splitAtEnd;
    addToArray((void**) &context->substitutions, &context->numSubstitutions,
            &info, sizeof(info));
    appendStringToStringBuffer(sb, value);
}

static void substituteExpansion(const char* word, struct StringBuffer* sb,
        struct ExpandContext* context, bool doubleQuoted) {
    char** fields;
    int flags = context->flags;
    if (doubleQuoted) flags |= EXPAND_NO_FIELD_SPLIT;
    ssize_t numFields = expand(word, flags, &fields);
    if (numFields < 0) err(1, "expand");

    for (ssize_t i = 0; i < numFields; i++) {
        substitute(fields[i], sb, context, true, i < numFields - 1);
        free(fields[i]);
        if (i < numFields - 1) {
            appendToStringBuffer(sb, ' ');
        }
    }
    free(fields);
}

static char* readOldCommandSubst(const char** word) {
    struct StringBuffer sb;
    initStringBuffer(&sb);

    bool escaped = false;
    while (true) {
        if (**word == '\\') {
            if (escaped) {
                appendToStringBuffer(&sb, '\\');
                escaped = false;
            } else {
                escaped = true;
            }
        } else if (**word == '`') {
            if (escaped) {
                appendToStringBuffer(&sb, '`');
                escaped = false;
            } else {
                appendToStringBuffer(&sb, '\n');
                (*word)++;
                return finishStringBuffer(&sb);
            }
        } else if (**word == '$') {
            appendToStringBuffer(&sb, '$');
            escaped = false;
        } else {
            if (escaped) {
                appendToStringBuffer(&sb, '\\');
                escaped = false;
            }
            appendToStringBuffer(&sb, **word);
        }

        (*word)++;
    }
}

static bool doCommandSubstitution(const char** word,
        struct StringBuffer* sb, struct ExpandContext* context,
        bool doubleQuoted, bool oldStyle) {
    struct Parser parser;
    initParser(&parser);
    struct CompleteCommand command;
    char* commandString = NULL;
    enum ParserResult result;

    if (oldStyle) {
        commandString = readOldCommandSubst(word);
        result = parseString(&parser, &command, commandString);
    } else {
        result = parseCommandSubstitution(&parser, word, NULL, &command);
    }

    if (result != PARSER_MATCH && result != PARSER_NO_CMD) {
        free(commandString);
        return false;
    }

    size_t bufferOffset = sb->used;
    if (result == PARSER_MATCH) {
        executeAndRead(&command, sb);
        // Remove newline characters at the end.
        while (sb->used > bufferOffset && sb->buffer[sb->used - 1] == '\n') {
            sb->used--;
        }
        freeCompleteCommand(&command);
    }

    struct SubstitutionInfo info;
    info.begin = bufferOffset;
    info.end = sb->used;
    info.startField = 0;
    info.endField = 0;
    info.applyFieldSplitting = !doubleQuoted;
    info.splitAtEnd = false;
    addToArray((void**) &context->substitutions, &context->numSubstitutions,
            &info, sizeof(info));

    freeParser(&parser);
    free(commandString);

    return true;
}

static ssize_t doDollarSubstitutions(const char* word, bool doubleQuoted,
        struct StringBuffer* sb, struct ExpandContext* context) {
    const char* begin = word;
    char c = *word;
    if (c == '{') {
        // TODO: Handle ${@} and ${*}.
        word++;

        if (word[0] == '}') {
            return -1;
        }

        bool varLength = false;
        if (word[0] == '#') {
            if (word[1] == '}' || word[1] == ':' || ((word[1] == '-' ||
                    word[1] == '=' || word[1] == '?' || word[1] == '+') &&
                    word[2] != '}')) {
                // The expansion refers to the $# variable.
            } else {
                varLength = true;
                word++;
            }
        }

        // TODO: Implement prefix and suffix removal.
        size_t nameLength = strcspn(word + 1, "+-:=?}") + 1;
        char* name = strndup(word, nameLength);
        if (!name) err(1, "strdup");

        if (!isRegularVariableName(name) && !isPositionalParameter(name) &&
                !(nameLength == 1 && isSpecialParameter(*name))) {
            free(name);
            return -1;
        }

        const char* value = getVariable(name);
        word += nameLength;

        void* toBeFreed = NULL;
        if (word[0] != '}') {
            bool nullMeansUnset = word[0] == ':';
            if (nullMeansUnset) word++;

            c = *word++;

            size_t braces = 0;
            size_t wordLength;
            for (wordLength = 0; true; wordLength++) {
                if (!word[wordLength]) {
                    free(name);
                    return -1;
                }
                if (word[wordLength] == '$' && word[wordLength + 1] == '{') {
                    braces++;
                }
                if (word[wordLength] == '}') {
                    if (braces == 0) break;
                    braces--;
                }
            }

            if (c == '-') {
                if (!value || (nullMeansUnset && !*value)) {
                    char* subst = strndup(word, wordLength);
                    if (!subst) err(1, "strdup");
                    substituteExpansion(subst, sb, context, doubleQuoted);
                    free(subst);
                }
            } else if (c == '=') {
                if (!value || (nullMeansUnset && !*value)) {
                    if (!isRegularVariableName(name)) {
                        free(name);
                        return -1;
                    }
                    char* subst = strndup(word, wordLength);
                    if (!subst) err(1, "strdup");
                    value = expandWord(subst);
                    setVariable(name, value, false);
                    toBeFreed = (void*) value;
                    free(subst);
                }
            } else if (c == '?') {
                if (!value || (nullMeansUnset && !*value)) {
                    char* message = NULL;
                    if (wordLength) {
                        char* subst = strndup(word, wordLength);
                        if (!subst) err(1, "strdup");
                        message = expandWord(subst);
                        free(subst);
                    }
                    if (message) {
                        warnx("%s: %s", name, message);
                        free(message);
                    } else {
                        warnx(value ? "%s: parameter is null" :
                                "%s: parameter is not set", name);
                    }
                    if (!shellOptions.interactive) exit(1);
                    free(name);
                    return -2;
                }
            } else if (c == '+') {
                if (value && (!nullMeansUnset || *value)) {
                    char* subst = strndup(word, wordLength);
                    if (!subst) err(1, "strdup");
                    substituteExpansion(subst, sb, context, doubleQuoted);
                    free(subst);
                    value = NULL;
                }
            }
            word += wordLength;
        }

        word++;
        free(name);
        char buffer[21];

        if (varLength) {
            snprintf(buffer, sizeof(buffer), "%zu",
                    value ? strlen(value) : (size_t) 0);
            value = buffer;
        }
        substitute(value, sb, context, doubleQuoted, false);
        free(toBeFreed);
    } else if (c == '(') {
        word++;
        if (!doCommandSubstitution(&word, sb, context, doubleQuoted, false)) {
            return -1;
        }
    } else if (c == '*' || c == '@') {
        bool splitting = !doubleQuoted &&
                !(context->flags & EXPAND_NO_FIELD_SPLIT);
        const char* ifs = getVariable("IFS");
        char sep = ifs ? ifs[0] : ' ';
        if ((splitting || c == '@') && !sep) sep = ' ';
        if (doubleQuoted && c == '@') {
            context->deleteIfEmpty = true;
        }

        for (int i = 1; i <= numArguments; i++) {
            substitute(arguments[i], sb, context, doubleQuoted,
                    i != numArguments && (splitting || c == '@'));
            if (i != numArguments && sep) {
                appendToStringBuffer(sb, sep);
            }
        }
        word++;
    } else if (isdigit(c) || isSpecialParameter(c)) {
        char name[2] = { c, '\0' };
        const char* value = getVariable(name);
        substitute(value, sb, context, doubleQuoted, false);
        word++;
    } else if (isalpha(c) || c == '_') {
        size_t nameLength = 1;
        while (isalnum(word[nameLength]) || word[nameLength] == '_') {
            nameLength++;
        }
        char* name = strndup(word, nameLength);
        if (!name) err(1, "strdup");
        const char* value = getVariable(name);
        free(name);
        substitute(value, sb, context, doubleQuoted, false);
        word += nameLength;
    } else {
        appendToStringBuffer(sb, '$');
    }

    return word - begin;
}

static char* doSubstitutions(const char* word, struct ExpandContext* context) {
    struct StringBuffer sb;
    initStringBuffer(&sb);

    bool escaped = false;
    bool singleQuote = false;
    bool doubleQuote = false;

    while (*word) {
        char c = *word++;

        if (!singleQuote && c == '\\') {
            escaped = !escaped;
        } else if (!escaped && !doubleQuote && c == '\'') {
            singleQuote = !singleQuote;
        } else if (!escaped && !singleQuote && c == '"') {
            doubleQuote = !doubleQuote;
        } else if (!escaped && !singleQuote && c == '$') {
            ssize_t length = doDollarSubstitutions(word, doubleQuote, &sb,
                    context);
            if (length < 0) {
                free(sb.buffer);
                if (length == -1) warnx("invalid substitution");
                return NULL;
            }
            word += length;
            continue;
        } else if (!escaped && !singleQuote && c == '`') {
            if (!doCommandSubstitution(&word, &sb, context, doubleQuote,
                    true)) {
                free(sb.buffer);
                warnx("invalid substitution");
                return NULL;
            }
            continue;
        }

        appendToStringBuffer(&sb, c);
        escaped = escaped && c == '\\';
    }

    return finishStringBuffer(&sb);
}

static size_t splitFields(char* word, struct ExpandContext* context,
        char*** result) {
    const char* ifs = getVariable("IFS");
    if (!ifs) ifs = " \t\n";

    char** fields = NULL;
    size_t numFields = 0;
    size_t fieldOffset = 0;
    size_t wordLength = strlen(word);

    for (size_t i = 0; i < context->numSubstitutions; i++) {
        struct SubstitutionInfo* subst = &context->substitutions[i];
        subst->begin -= fieldOffset;
        subst->startField = numFields;
        size_t splitBegin = subst->begin;
        while (subst->applyFieldSplitting && fieldOffset < subst->end) {
            size_t length = splitBegin + strcspn(word + fieldOffset +
                    splitBegin, ifs);
            splitBegin = 0;
            if (fieldOffset + length >= subst->end) break;
            if (fieldOffset + length != 0) {
                char* field = word + fieldOffset;
                addToArray((void**) &fields, &numFields, &field, sizeof(char*));
            }
            fieldOffset += length;
            bool nonWhitespace = !isspace(word[fieldOffset]);
            word[fieldOffset++] = '\0';

            length = strspn(word + fieldOffset, ifs);
            for (size_t j = 0; j < length; j++) {
                if (!isspace(word[fieldOffset + j])) {
                    if (nonWhitespace) {
                        word[fieldOffset + j] = '\0';
                        char* field = word + fieldOffset + j;
                        addToArray((void**) &fields, &numFields, &field,
                                sizeof(char*));
                    } else {
                        nonWhitespace = true;
                    }
                }
            }
            fieldOffset += length;
        }
        subst->endField = numFields;
        subst->end -= fieldOffset;

        if (subst->splitAtEnd) {
            char* field = word + fieldOffset;
            addToArray((void**) &fields, &numFields, &field, sizeof(char*));
            fieldOffset += subst->end;
            word[fieldOffset++] = '\0';
        }
    }

    if (fieldOffset != wordLength) {
        char* field = word + fieldOffset;
        addToArray((void**) &fields, &numFields, &field, sizeof(char*));
    }

    *result = fields;
    return numFields;
}

static bool isSpecialInDoubleQuotes(char c) {
    return c == '$' || c == '`' || c == '\\' || c == '"';
}

char* removeQuotes(const char* word, size_t fieldIndex,
        struct SubstitutionInfo* substitutions, size_t numSubstitutions) {
    size_t substIndex = 0;
    struct SubstitutionInfo* subst = substitutions;

    bool escaped = false;
    bool singleQuote = false;
    bool doubleQuote = false;

    struct StringBuffer buffer;
    initStringBuffer(&buffer);

    for (size_t i = 0; word[i]; i++) {
        while (subst && !(fieldIndex < subst->endField ||
                (fieldIndex == subst->endField && i < subst->end))) {
            substIndex++;
            if (substIndex < numSubstitutions) {
                subst = &substitutions[substIndex];
            } else {
                subst = NULL;
            }
        }
        char c = word[i];
        if (subst && (fieldIndex > subst->startField ||
                (fieldIndex == subst->startField && i >= subst->begin))) {
            // No quote removal for substitution results.
        } else if (!escaped) {
            if (!singleQuote && c == '\\' && (!doubleQuote ||
                    isSpecialInDoubleQuotes(word[i + 1]))) {
                escaped = true;
                continue;
            } else if (!doubleQuote && c == '\'') {
                singleQuote = !singleQuote;
                continue;
            } else if (!singleQuote && c == '"') {
                doubleQuote = !doubleQuote;
                continue;
            }
        }

        escaped = false;
        appendToStringBuffer(&buffer, c);
    }
    return finishStringBuffer(&buffer);
}
