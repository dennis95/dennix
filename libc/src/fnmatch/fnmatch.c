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

/* libc/src/fnmatch/fnmatch.c
 * Pattern matching.
 */

#include <fnmatch.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static size_t getBracketExpressionLength(const char* pattern, size_t length) {
    size_t i = 0;
    if (i < length && (pattern[i] == '!' || pattern[i] == '^')) {
        i++;
    }
    if (i < length && pattern[i] == ']') {
        i++;
    }
    while (i < length) {
        if (pattern[i] == ']') {
            return i;
        }
        if (pattern[i] == '[' && i + 1 < length) {
            if (pattern[i + 1] == '.' || pattern[i + 1] == '=' ||
                    pattern[i + 1] == ':') {
                char end = pattern[i + 1];
                i += 2;

                while (i + 1 < length) {
                    if (pattern[i] == end && pattern[i + 1] == ']') {
                        i++;
                        break;
                    }
                    i++;
                }
            }
        }

        i++;
    }

    return 0;
}

static wint_t getWideChar(const char* string, size_t length,
        size_t* charLength) {
    // This assumes an ASCII-based encoding like UTF-8.
    if ((unsigned char) *string <= 127) {
        *charLength = 1;
        return *string;
    }
    mbstate_t ps = {0};
    wchar_t wc;
    size_t result = mbrtowc(&wc, string, length, &ps);
    if (result > length) {
        *charLength = 1;
        return WEOF;
    }
    *charLength = result;
    return wc;
}

static wint_t casefold(wint_t wc) {
    return iswupper(wc) ? towlower(wc) : towupper(wc);
}

static bool matchBracketExpression(const char* bracketExpression,
        size_t expressionLength, const char* string, size_t stringLength,
        size_t* charLength, bool caseInsensitive) {
    wint_t wc = getWideChar(string, stringLength, charLength);
    if (wc == WEOF) return false;
    wint_t folded = caseInsensitive ? casefold(wc) : wc;

    size_t i = 0;
    bool nonmatching = false;
    if (bracketExpression[i] == '!' || bracketExpression[i] == '^') {
        nonmatching = true;
        i++;
    }

    wint_t previousChar = WEOF;

    while (i < expressionLength) {
        bool matched = false;
        if (bracketExpression[i] == '[' && i + 1 < expressionLength) {
            if (bracketExpression[i + 1] == '.' ||
                    bracketExpression[i + 1] == '=') {
                // We only support the collating elements and equivalence class
                // of the POSIX locale.
                char end = bracketExpression[i + 1];
                i += 2;
                size_t length;
                wint_t patternChar = getWideChar(bracketExpression + i,
                        expressionLength - i, &length);
                if (i + length + 1 < expressionLength &&
                        bracketExpression[i + length] == end &&
                        bracketExpression[i + length + 1] == ']') {
                    matched = (wc == patternChar || folded == patternChar);
                    previousChar = patternChar;
                } else {
                    previousChar = WEOF;
                }
                char endString[] = { end, ']', '\0' };
                i += strstr(bracketExpression + i, endString) -
                        (bracketExpression + i) + 2;
            } else if (bracketExpression[i + 1] == ':') {
                i += 2;
                char charclass[8];
                const char* classStart = bracketExpression + i;
                const char* classEnd = strstr(classStart, ":]");
                size_t classLength = classEnd - classStart;
                if (classLength < 8) {
                    strlcpy(charclass, classStart, classLength + 1);
                    wctype_t type = wctype(charclass);
                    matched = iswctype(wc, type) || iswctype(folded, type);
                }
                i += classLength + 2;
                previousChar = WEOF;
            } else {
                matched = *string == '[';
                i++;
                previousChar = L'[';
            }
        } else if (bracketExpression[i] == '-' && previousChar != WEOF &&
                i + 1 < expressionLength) {
            wint_t start = previousChar;
            size_t length;
            i++;
            bool collatingSymbol = false;
            if (i + 1 < expressionLength && bracketExpression[i] == '[' &&
                    bracketExpression[i + 1] == '.') {
                collatingSymbol = true;
                i += 2;
            }
            wint_t end = getWideChar(bracketExpression + i,
                    expressionLength - i, &length);
            if (collatingSymbol) {
                if (i + length + 1 < expressionLength &&
                        bracketExpression[i + length] == '.' &&
                        bracketExpression[i + length + 1] == ']') {
                    // A valid collating symbol.
                } else {
                    end = WEOF;
                }
                length = strstr(bracketExpression + i, ".]") -
                        (bracketExpression + i) + 2;
            }
            if (end != WEOF) {
                matched = (start <= wc && wc <= end) ||
                        (start <= folded && folded <= end);
            }
            previousChar = WEOF;
            i += length;
        } else {
            size_t length;
            wint_t patternChar = getWideChar(bracketExpression + i,
                    expressionLength - i, &length);
            matched = (wc == patternChar || folded == patternChar);
            i += length;
            previousChar = patternChar;
        }

        if (matched) {
            return !nonmatching;
        }
    }

    return nonmatching;
}

static int match(const char* pattern, size_t patternLength,
        const char* string, size_t stringLength, int flags) {
    size_t subpatternStart = 0;
    size_t substringStart = 0;

    bool escaped = false;
    size_t patternOffset = 0;
    size_t stringOffset = 0;

    if (flags & FNM_PERIOD && stringLength > 0 && string[0] == '.') {
        if (pattern[0] != '.') return FNM_NOMATCH;
        stringOffset++;
        patternOffset++;
    }

    if (patternLength == 0) {
        if (stringLength == 0) return 0;
        return FNM_NOMATCH;
    }

    while (patternOffset < patternLength) {
        if (pattern[patternOffset] == '\\' && !(flags & FNM_NOESCAPE)
                && !escaped) {
            escaped = true;
            patternOffset++;
        } else if (pattern[patternOffset] == '?' && !escaped) {
            if (stringOffset >= stringLength) return FNM_NOMATCH;
            size_t charLength;
            wchar_t wc = getWideChar(string + stringOffset,
                    stringLength - stringOffset, &charLength);
            if (wc == WEOF) {
                if (subpatternStart == 0) return FNM_NOMATCH;
                patternOffset = subpatternStart;
                substringStart++;
                stringOffset = substringStart;
                continue;
            }
            stringOffset += charLength;
            patternOffset++;
        } else if (pattern[patternOffset] == '[' && !escaped) {
            if (stringOffset >= stringLength) return FNM_NOMATCH;
            const char* bracketExpression = pattern + patternOffset + 1;
            size_t length = getBracketExpressionLength(bracketExpression,
                    patternLength - patternOffset - 1);
            if (length == 0) {
                // Not a valid bracket expression.
                if (stringOffset >= stringLength) return FNM_NOMATCH;
                if (string[stringOffset] != '[') {
                    if (subpatternStart == 0) return FNM_NOMATCH;
                    patternOffset = subpatternStart;
                    substringStart++;
                    stringOffset = substringStart;
                    continue;
                }
                stringOffset++;
                patternOffset++;
            } else {
                size_t charLength;
                if (matchBracketExpression(bracketExpression, length,
                        string + stringOffset, stringLength - stringOffset,
                        &charLength, flags & FNM_CASEFOLD)) {
                    stringOffset += charLength;
                    patternOffset += length + 2;
                } else {
                    if (subpatternStart == 0) return FNM_NOMATCH;
                    patternOffset = subpatternStart;
                    substringStart++;
                    stringOffset = substringStart;
                    continue;
                }
            }
        } else if (pattern[patternOffset] == '*' && !escaped) {
            patternOffset++;
            subpatternStart = patternOffset;
            substringStart = stringOffset;
        } else {
            // Match a literal character.
            if (stringOffset >= stringLength) return FNM_NOMATCH;
            size_t patternCharLength;
            wint_t patternChar = getWideChar(pattern + patternOffset,
                    patternLength - patternOffset, &patternCharLength);
            if (patternChar == WEOF) return FNM_NOMATCH;
            size_t charLength;
            wint_t wc = getWideChar(string + stringOffset,
                    stringLength - stringOffset, &charLength);
            wint_t folded = (flags & FNM_CASEFOLD) ? casefold(wc) : wc;

            if (wc != patternChar && folded != patternChar) {
                if (subpatternStart == 0) return FNM_NOMATCH;
                patternOffset = subpatternStart;
                substringStart++;
                stringOffset = substringStart;
                continue;
            }
            stringOffset += charLength;
            escaped = false;
            patternOffset += patternCharLength;
        }
    }

    if (patternOffset == subpatternStart) {
        return 0;
    }

    if (escaped && !(flags & FNM_PATHNAME)) {
        if (stringOffset != stringLength - 1) return FNM_NOMATCH;
        if (string[stringOffset] != '\\') return FNM_NOMATCH;
        return 0;
    }

    if (stringOffset == stringLength) {
        return 0;
    }
    return FNM_NOMATCH;
}

int fnmatch(const char* pattern, const char* string, int flags) {
    if (flags & FNM_PATHNAME) {
        while (true) {
            size_t length = strcspn(string, "/");
            size_t patternLength = strcspn(pattern, "/");
            if (pattern[patternLength] == '\0') {
                flags &= ~FNM_PATHNAME;
            }

            if (match(pattern, patternLength, string, length, flags) != 0) {
                return FNM_NOMATCH;
            }

            if (string[length] == '\0' && pattern[patternLength] == '\0') {
                return 0;
            }
            if (string[length] == '\0' || pattern[patternLength] == '\0') {
                return FNM_NOMATCH;
            }

            pattern += patternLength + 1;
            string += length + 1;
        }
    }
    return match(pattern, strlen(pattern), string, strlen(string), flags);
}
