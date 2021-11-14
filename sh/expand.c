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

/* sh/expand.c
 * Word expansion.
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expand.h"
#include "stringbuffer.h"
#include "variables.h"

struct SubstitutionInfo {
    size_t begin;
    size_t end;
    size_t startField;
    size_t endField;
    bool applyFieldSplitting;
    bool splitAtEnd;
};

struct Context {
    struct SubstitutionInfo* substitutions;
    size_t numSubstitutions;
    int flags;
    bool deleteIfEmpty;
};

static ssize_t doParameterSubstitution(const char* word, bool doubleQuoted,
        struct StringBuffer* sb, struct Context* context);
static char* doSubstitutions(const char* word, struct Context* context);
static size_t splitFields(char* word, struct Context* context, char*** result);
static void removeQuotes(char** fields, struct Context* context,
        size_t numFields);

char* expandWord(const char* word) {
    char** fields;
    ssize_t numFields = expand(word, EXPAND_NO_FIELD_SPLIT, &fields);
    if (numFields < 0) return NULL;
    assert(numFields == 1);
    char* result = fields[0];
    free(fields);
    return result;
}

ssize_t expand(const char* word, int flags, char*** result) {
    struct Context context;
    context.substitutions = NULL;
    context.numSubstitutions = 0;
    context.flags = flags;
    context.deleteIfEmpty = false;

    char* str = doSubstitutions(word, &context);
    if (!str) {
        free(context.substitutions);
        return -1;
    }

    char** fields;
    ssize_t numFields;
    if (flags & EXPAND_NO_FIELD_SPLIT) {
        fields = malloc(sizeof(char*));
        if (!fields) err(1, "malloc");
        fields[0] = str;
        numFields = 1;
    } else {
        numFields = splitFields(str, &context, &fields);
    }

    removeQuotes(fields, &context, numFields);

    if (context.deleteIfEmpty && numFields == 1 && *fields[0] == '\0' &&
            context.numSubstitutions == 0) {
        free(fields[0]);
        numFields = 0;
    }

    free(context.substitutions);
    free(str);
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
        struct Context* context, bool doubleQuoted, bool splitAtEnd) {
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
        struct Context* context, bool doubleQuoted) {
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

static ssize_t doParameterSubstitution(const char* word, bool doubleQuoted,
        struct StringBuffer* sb, struct Context* context) {
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

static char* doSubstitutions(const char* word, struct Context* context) {
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
            ssize_t length = doParameterSubstitution(word, doubleQuote, &sb,
                    context);
            if (length < 0) {
                free(sb.buffer);
                if (length == -1) warnx("invalid substitution");
                return NULL;
            }
            word += length;
            continue;
        }

        appendToStringBuffer(&sb, c);
        escaped = escaped && c == '\\';
    }

    return finishStringBuffer(&sb);
}

static size_t splitFields(char* word, struct Context* context,
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

static void removeQuotes(char** fields, struct Context* context,
        size_t numFields) {
    size_t substIndex = 0;
    struct SubstitutionInfo* subst = context->substitutions;

    bool escaped = false;
    bool singleQuote = false;
    bool doubleQuote = false;

    for (size_t i = 0; i < numFields; i++) {
        struct StringBuffer buffer;
        initStringBuffer(&buffer);
        const char* word = fields[i];

        for (size_t j = 0; word[j]; j++) {
            while (subst && !(i < subst->endField || (i == subst->endField &&
                    j < subst->end))) {
                substIndex++;
                if (substIndex < context->numSubstitutions) {
                    subst = &context->substitutions[substIndex];
                } else {
                    subst = NULL;
                }
            }
            char c = word[j];
            if (subst && (i > subst->startField || (i == subst->startField &&
                    j >= subst->begin))) {
                // No quote removal for substitution results.
            } else if (!escaped) {
                if (!singleQuote && c == '\\') {
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
        fields[i] = finishStringBuffer(&buffer);
    }
}
