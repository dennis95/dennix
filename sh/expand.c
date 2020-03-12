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

/* sh/expand.c
 * Word expansion.
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expand.h"
#include "stringbuffer.h"
#include "variables.h"

static ssize_t doParameterSubstitution(const char* word,
        struct StringBuffer* sb);
static char* doSubstitutions(const char* word);
static char* removeQuotes(const char* word);

char* expandWord(const char* word) {
    char* str = doSubstitutions(word);
    if (!str) return NULL;
    char* result = removeQuotes(str);
    free(str);
    return result;
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

static ssize_t doParameterSubstitution(const char* word,
        struct StringBuffer* sb) {
    const char* begin = word;
    char c = *word;
    if (c == '{') {
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
                    value = expandWord(subst);
                    toBeFreed = (void*) value;
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
                    value = expandWord(subst);
                    toBeFreed = (void*) value;
                    free(subst);
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
        if (value) {
            if (!appendStringToStringBuffer(sb, value)) err(1, "stringbuffer");
        }
        free(toBeFreed);
    } else if (isdigit(c) || isSpecialParameter(c)) {
        char name[2] = { c, '\0' };
        const char* value = getVariable(name);
        if (value) {
            if (!appendStringToStringBuffer(sb, value)) err(1, "stringbuffer");
        }
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
        if (value) {
            if (!appendStringToStringBuffer(sb, value)) err(1, "stringbuffer");
        }
        word += nameLength;
    } else {
        if (!appendToStringBuffer(sb, '$')) err(1, "stringbuffer");
    }

    return word - begin;
}

static char* doSubstitutions(const char* word) {
    struct StringBuffer sb;
    if (!initStringBuffer(&sb)) err(1, "initStringBuffer");

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
            ssize_t length = doParameterSubstitution(word, &sb);
            if (length < 0) {
                free(sb.buffer);
                if (length == -1) warnx("invalid substitution");
                return NULL;
            }
            word += length;
            continue;
        }

        if (!appendToStringBuffer(&sb, c)) err(1, "stringbuffer");
        escaped = escaped && c == '\\';
    }

    return finishStringBuffer(&sb);
}

static char* removeQuotes(const char* word) {
    struct StringBuffer buffer;
    if (!initStringBuffer(&buffer)) err(1, "initStringBuffer");

    bool escaped = false;
    bool singleQuote = false;
    bool doubleQuote = false;

    while (*word) {
        char c = *word++;

        if (!escaped) {
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
        if (!appendToStringBuffer(&buffer, c)) err(1, "stringbuffer");
    }

    return finishStringBuffer(&buffer);
}
