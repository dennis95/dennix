/* Copyright (c) 2022 Dennis Wölfing
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

/* utils/sort.c
 * Sorting.
 */

#include "utils.h"
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct InputLines {
    char** lines;
    size_t numLines;
    size_t linesAllocated;
};

enum Modifiers {
    MOD_IGNORE_BLANK_AT_START = 1 << 0,
    MOD_IGNORE_BLANK_AT_END = 1 << 1,
    MOD_DICTIONARY_ORDER = 1 << 2,
    MOD_IGNORE_CASE = 1 << 3,
    MOD_IGNORE_NONPRINTING = 1 << 4,
    MOD_NUMERIC = 1 << 5,
    MOD_REVERSE = 1 << 6,
};

struct Key {
    unsigned long long fieldStart;
    unsigned long long firstCharacter;
    unsigned long long fieldEnd;
    unsigned long long lastCharacter;
    enum Modifiers modifiers;
};

static struct Key defaultKey = {
    .fieldStart = 1,
    .firstCharacter = 1,
    .fieldEnd = ULLONG_MAX,
    .lastCharacter = 0,
    .modifiers = 0
};

static struct Key fallbackKey = {
    .fieldStart = 1,
    .firstCharacter = 1,
    .fieldEnd = ULLONG_MAX,
    .lastCharacter = 0,
    .modifiers = 0
};

static char fieldSeparator = '\0';
static struct Key* keys = NULL;
static size_t numKeys = 0;
static bool unique = false;

static void addKey(const char* keyString);
static int compare(const void* a, const void* b);
static int compareKeyFields(const char* line1, const char* line2,
        struct Key key);
static int compareLines(const char* line1, const char* line2);
static int compareNumeric(const char* keyField1, size_t length1,
        const char* keyField2, size_t length2);
static int compareUsingKey(const char* line1, const char* line2,
        struct Key key);
static size_t getFieldPos(const char* field, size_t pos, bool atEnd);
static void getKeyField(const char* line, struct Key key, const char** keyStart,
        size_t* keyLength);
static bool isFieldSeparator(char c);
static bool isSignificantChar(unsigned char c, struct Key key);
static enum Modifiers parseModfiers(const char** str, bool startField);
static char* readLine(FILE* file, const char* filename);
static void readAllLines(struct InputLines* inputLines, FILE* file,
        const char* filename);

int main(int argc, char* argv[]) {
    bool check = false;
    bool checkWarn = false;
    enum Modifiers defaultModifiers = 0;
    const char* outputPath = NULL;

    struct option longopts[] = {
        { "ignore-leading-blanks", no_argument, 0, 'b' },
        { "check", no_argument, 0, 'c' },
        { "dictionary-order", no_argument, 0, 'd' },
        { "ignore-case", no_argument, 0, 'f' },
        { "ignore-nonprinting", no_argument, 0, 'i' },
        { "key", required_argument, 0, 'k' },
        { "merge", no_argument, 0, 'm' },
        { "numeric-sort", no_argument, 0, 'n' },
        { "output", required_argument, 0, 'o' },
        { "reverse", no_argument, 0, 'r' },
        { "field-separator", required_argument, 0, 't' },
        { "unique", no_argument, 0, 'u' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };
    const char* shortopts = "bcCdfik:mno:rt:u";

    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] [FILE...]\n"
                    "  -b, --ignore-leading-blanks ignore leading blanks\n"
                    "  -c, --check              check for sorted input\n"
                    "  -C                       check without warning\n"
                    "  -d, --dictionary-order   only blanka and alphanumeric\n"
                    "  -f, --ignore-case        convert to upper case\n"
                    "  -i, --ignore-nonprinting ignore nonprinting characters\n"
                    "  -k, --key=KEYDEF         sort via the given key\n"
                    "  -m, --merge              merge sorted files\n"
                    "  -n, --numeric-sort       sort numbers\n"
                    "  -o, --output=OUTPUT      write to OUTPUT\n"
                    "  -r, --reverse            reverse order\n"
                    "  -t, --field-separator=SEP use SEP as field separator\n"
                    "  -u, --unique             unique lines only\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'b':
            defaultModifiers |= MOD_IGNORE_BLANK_AT_START |
                    MOD_IGNORE_BLANK_AT_END;
            break;
        case 'c':
            check = true;
            checkWarn = true;
            break;
        case 'C':
            check = true;
            break;
        case 'd':
            defaultModifiers |= MOD_DICTIONARY_ORDER;
            break;
        case 'f':
            defaultModifiers |= MOD_IGNORE_CASE;
            break;
        case 'i':
            defaultModifiers |= MOD_IGNORE_NONPRINTING;
            break;
        case 'k':
            addKey(optarg);
            break;
        case 'm':
            // Ignored.
            break;
        case 'n':
            defaultModifiers |= MOD_NUMERIC;
            break;
        case 'o':
            outputPath = optarg;
            break;
        case 'r':
            defaultModifiers |= MOD_REVERSE;
            break;
        case 't':
            fieldSeparator = *optarg;
            break;
        case 'u':
            unique = true;
            break;
        case '?':
            return 2;
        }
    }

    for (size_t i = 0; i < numKeys; i++) {
        if (keys[i].modifiers == 0) {
            keys[i].modifiers = defaultModifiers;
        }
    }

    defaultKey.modifiers = defaultModifiers;
    fallbackKey.modifiers = defaultModifiers & MOD_REVERSE;

    if (check) {
        FILE* file;
        const char* filename;
        if (optind >= argc) {
            file = stdin;
            filename = "<stdin>";
        } else if (optind + 1 == argc) {
            if (strcmp(argv[optind], "-") == 0) {
                file = stdin;
                filename = "<stdin>";
            } else {
                file = fopen(argv[optind], "r");
                filename = argv[optind];
                if (!file) err(2, "'%s'", filename);
            }
        } else {
            errx(2, "extra operand");
        }

        char* currentLine = readLine(file, filename);
        size_t lineNumber = 0;

        while (true) {
            char* prevLine = currentLine;
            currentLine = readLine(file, filename);
            if (!currentLine) return 0;

            int comparison = compareLines(prevLine, currentLine);
            if (comparison > 0) {
                if (checkWarn) {
                    warnx("Lines %zu and %zu are in wrong order",
                            lineNumber, lineNumber + 1);
                }
                return 1;
            } else if (unique && comparison == 0) {
                if (checkWarn) {
                    warnx("Lines %zu and %zu are equal", lineNumber,
                            lineNumber + 1);
                }
                return 1;
            }

            free(prevLine);
            lineNumber++;
        }
    } else {
        struct InputLines input;
        input.lines = malloc(20 * sizeof(char*));
        if (!input.lines) err(2, "malloc");
        input.linesAllocated = 20;
        input.numLines = 0;

        if (optind >= argc) {
            readAllLines(&input, stdin, "<stdin>");
        } else {
            for (int i = optind; i < argc; i++) {
                if (strcmp(argv[i], "-") == 0) {
                    readAllLines(&input, stdin, "<stdin>");
                } else {
                    FILE* file = fopen(argv[i], "r");
                    if (!file) err(2, "'%s'", argv[i]);
                    readAllLines(&input, file, argv[i]);
                    fclose(file);
                }
            }
        }

        qsort(input.lines, input.numLines, sizeof(char*), compare);

        FILE* outputFile = stdout;
        if (outputPath) {
            outputFile = fopen(outputPath, "w");
            if (!outputFile) err(2, "'%s'", outputPath);
        }

        for (size_t i = 0; i < input.numLines; i++) {
            if (unique && i > 0) {
                if (compareLines(input.lines[i - 1], input.lines[i]) == 0) {
                    continue;
                }
            }
            fputs(input.lines[i], outputFile);
            fputc('\n', outputFile);
        }
    }
}

static void addKey(const char* keyString) {
    const char* str = keyString;
    struct Key key;

    char* end;
    errno = 0;
    key.fieldStart = strtoull(str, &end, 10);
    if (errno) err(2, "invalid key '%s'", keyString);
    str = end;

    if (*str == '.') {
        str++;
        key.firstCharacter = strtoull(str, &end, 10);
        if (errno) err(2, "invalid key '%s'", keyString);
        str = end;
    } else {
        key.firstCharacter = 1;
    }

    key.modifiers = parseModfiers(&str, true);

    if (*str == ',') {
        str++;

        key.fieldEnd = strtoull(str, &end, 10);
        if (errno) err(2, "invalid key '%s'", keyString);
        str = end;

        if (*str == '.') {
            str++;
            key.lastCharacter = strtoull(str, &end, 10);
            if (errno || end == str) err(2, "invalid key '%s'", keyString);
            str = end;
        } else {
            key.lastCharacter = 0;
        }

        key.modifiers |= parseModfiers(&str, false);
    } else {
        key.fieldEnd = ULLONG_MAX;
        key.lastCharacter = 0;
    }

    if (*str != '\0' || key.fieldStart == 0 || key.fieldEnd == 0 ||
            key.firstCharacter == 0) {
        errx(2, "invalid key '%s'", keyString);
    }

    keys = reallocarray(keys, numKeys + 1, sizeof(struct Key));
    if (!keys) err(2, "realloc");
    keys[numKeys] = key;
    numKeys++;
}

static int compare(const void* a, const void* b) {
    const char* line1 = *(const char**) a;
    const char* line2 = *(const char**) b;

    return compareLines(line1, line2);
}

static int compareKeyFields(const char* line1, const char* line2,
        struct Key key) {
    const char* keyField1;
    size_t length1;
    getKeyField(line1, key, &keyField1, &length1);

    const char* keyField2;
    size_t length2;
    getKeyField(line2, key, &keyField2, &length2);

    if (key.modifiers & MOD_NUMERIC) {
        return compareNumeric(keyField1, length1, keyField2, length2);
    }

    size_t index1 = 0;
    size_t index2 = 0;

    while (true) {
        while (index1 < length1 && !isSignificantChar(keyField1[index1], key)) {
            index1++;
        }

        while (index2 < length2 && !isSignificantChar(keyField2[index2], key)) {
            index2++;
        }

        if (index1 >= length1 || index2 >= length2) {
            if (index1 < length1) {
                return 1;
            } else if (index2 < length2) {
                return -1;
            }

            return 0;
        }

        unsigned char c1 = keyField1[index1];
        unsigned char c2 = keyField2[index2];

        if (key.modifiers & MOD_IGNORE_CASE) {
            c1 = toupper(c1);
            c2 = toupper(c2);
        }

        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }

        index1++;
        index2++;
    }
}

static int compareLines(const char* line1, const char* line2) {
    for (size_t i = 0; i < numKeys; i++) {
        int comparison = compareUsingKey(line1, line2, keys[i]);
        if (comparison != 0) return comparison;
    }

    if (numKeys == 0) {
        int comparison = compareUsingKey(line1, line2, defaultKey);
        if (comparison != 0) return comparison;
    }

    if (!unique && (numKeys != 0 ||
            defaultKey.modifiers != fallbackKey.modifiers)) {
        return compareUsingKey(line1, line2, fallbackKey);
    }

    return 0;
}

static int compareNumeric(const char* keyField1, size_t length1,
        const char* keyField2, size_t length2) {
    size_t index1 = 0;
    size_t index2 = 0;

    while (index1 < length1 && isblank((unsigned char) keyField1[index1])) {
        index1++;
    }
    while (index2 < length2 && isblank((unsigned char) keyField2[index2])) {
        index2++;
    }

    bool negative1 = false;
    bool negative2 = false;
    if (index1 < length1 && keyField1[index1] == '-') {
        negative1 = true;
        index1++;
    }
    if (index2 < length2 && keyField2[index2] == '-') {
        negative2 = true;
        index2++;
    }

    while (index1 < length1 && keyField1[index1] == '0') {
        index1++;
    }
    while (index2 < length2 && keyField2[index2] == '0') {
        index2++;
    }

    size_t numberBegin1 = index1;
    size_t numberBegin2 = index2;

    while (index1 < length1 && isdigit((unsigned char) keyField1[index1])) {
        index1++;
    }
    while (index2 < length2 && isdigit((unsigned char) keyField2[index2])) {
        index2++;
    }

    size_t magnitude1 = index1 - numberBegin1;
    size_t magnitude2 = index2 - numberBegin2;

    bool zero1 = magnitude1 == 0;
    bool zero2 = magnitude2 == 0;

    size_t decimalPlaces1 = 0;
    size_t decimalPlaces2 = 0;

    if (index1 < length1 && keyField1[index1] == '.') {
        index1++;
         while (index1 < length1 &&
                isdigit((unsigned char) keyField1[index1])) {
            if (keyField1[index1] != '0') {
                zero1 = false;
            }
            index1++;
            decimalPlaces1++;
        }
    }

    if (index2 < length2 && keyField2[index2] == '.') {
        index2++;
         while (index2 < length2 &&
                isdigit((unsigned char) keyField2[index2])) {
            if (keyField2[index2] != '0') {
                zero2 = false;
            }
            index2++;
            decimalPlaces2++;
        }
    }

    if (zero1 && zero2) {
        return 0;
    } else if (negative1 && !zero1 && (!negative2 || zero2)) {
        return -1;
    } else if (negative2 && !zero2 && (!negative1 || zero1)) {
        return 1;
    } else if (zero1) {
        return -1;
    } else if (zero2) {
        return 1;
    }

    // Both numbers are non-zero with the same sign.
    int sign = negative1 ? -1 : 1;

    if (magnitude1 < magnitude2) {
        return -sign;
    } else if (magnitude1 > magnitude2) {
        return sign;
    }

    for (size_t i = 0; i < magnitude1; i++) {
        char digit1 = keyField1[numberBegin1 + i];
        char digit2 = keyField2[numberBegin2 + i];

        if (digit1 < digit2) {
            return -sign;
        } else if (digit1 > digit2) {
            return sign;
        }
    }

    index1 -= decimalPlaces1;
    index2 -= decimalPlaces2;

    size_t maxDecimals = decimalPlaces1 > decimalPlaces2 ? decimalPlaces1 :
            decimalPlaces2;

    for (size_t i = 0; i < maxDecimals; i++) {
        char digit1 = i < decimalPlaces1 ? keyField1[index1 + i] : '0';
        char digit2 = i < decimalPlaces2 ? keyField2[index2 + i] : '0';

        if (digit1 < digit2) {
            return -sign;
        } else if (digit1 > digit2) {
            return sign;
        }
    }

    return 0;
}

static int compareUsingKey(const char* line1, const char* line2,
        struct Key key) {
    int comparison = compareKeyFields(line1, line2, key);
    return key.modifiers & MOD_REVERSE ? -comparison : comparison;
}

static size_t getFieldPos(const char* field, size_t pos, bool atEnd) {
    bool nonBlankSeen = false;
    for (size_t i = 1; i < pos || pos == 0; i++) {
        bool isSeparator = false;
        if (fieldSeparator) {
            isSeparator = field[i - 1] == fieldSeparator;
        } else if (isblank((unsigned char) field[i - 1])) {
            isSeparator = nonBlankSeen;
        } else {
            nonBlankSeen = true;
        }

        if (field[i - 1] == '\0' || isSeparator) {
            return i - 1;
        }
    }
    return pos - 1 + atEnd;
}

static void getKeyField(const char* line, struct Key key, const char** keyStart,
        size_t* keyLength) {
    size_t fieldIndex = 1;

    while (fieldIndex < key.fieldStart) {
        if (!fieldSeparator) {
            while (isblank((unsigned char) *line)) {
                line++;
            }
        }

        while (*line != '\0' && !isFieldSeparator(*line)) {
            line++;
        }

        if (fieldSeparator && *line == fieldSeparator) {
            line++;
        }

        fieldIndex++;
    }

    const char* fieldStart = line;

    if (key.modifiers & MOD_IGNORE_BLANK_AT_START) {
        while (isblank((unsigned char) *line)) {
            line++;
        }
    }

    line += getFieldPos(line, key.firstCharacter, false);

    *keyStart = line;

    while (*line != '\0' && fieldIndex < key.fieldEnd) {
        if (!fieldSeparator) {
            while (isblank((unsigned char) *line)) {
                line++;
            }
        }

        while (*line != '\0' && !isFieldSeparator(*line)) {
            line++;
        }

        if (fieldSeparator && *line == fieldSeparator) {
            line++;
        }

        fieldIndex++;
    }

    if (fieldIndex > key.fieldEnd) {
        *keyLength = 0;
        return;
    }

    if (key.fieldStart != key.fieldEnd) {
        fieldStart = line;
    }

    if (key.modifiers & MOD_IGNORE_BLANK_AT_END) {
        while (isblank((unsigned char) *fieldStart)) {
            fieldStart++;
        }
    }

    line = fieldStart + getFieldPos(fieldStart, key.lastCharacter, true);
    if (line <= *keyStart) {
        *keyLength = 0;
        return;
    }

    *keyLength = line - *keyStart;
}

static bool isFieldSeparator(char c) {
    if (fieldSeparator) {
        return c == fieldSeparator;
    }
    return isblank((unsigned char) c);
}

static bool isSignificantChar(unsigned char c, struct Key key) {
    return (!(key.modifiers & MOD_DICTIONARY_ORDER) || isalnum(c) || isblank(c))
            && (!(key.modifiers & MOD_IGNORE_NONPRINTING) || isprint(c));
}

static enum Modifiers parseModfiers(const char** str, bool startField) {
    enum Modifiers result = 0;

    while (**str != ',' && **str != '\0') {
        switch (**str) {
        case 'b':
            if (startField) {
                result |= MOD_IGNORE_BLANK_AT_START;
            } else {
                result |= MOD_IGNORE_BLANK_AT_END;
            }
            break;
        case 'd': result |= MOD_DICTIONARY_ORDER; break;
        case 'f': result |= MOD_IGNORE_CASE; break;
        case 'i': result |= MOD_IGNORE_NONPRINTING; break;
        case 'n': result |= MOD_NUMERIC; break;
        case 'r': result |= MOD_REVERSE; break;
        default:
            errx(2, "invalid modifier '%c'", **str);
        }

        (*str)++;
    }

    return result;
}

static char* readLine(FILE* file, const char* filename) {
    char* line = NULL;
    size_t size = 0;
    ssize_t length = getline(&line, &size, file);
    if (length < 0) {
        if (feof(file)) return NULL;
        err(2, "'%s'", filename);
    } else if (line[length - 1] == '\n') {
        line[length - 1] = '\0';
    }
    return line;
}

static void readAllLines(struct InputLines* inputLines, FILE* file,
        const char* filename) {
    while (true) {
        if (inputLines->numLines >= inputLines->linesAllocated) {
            inputLines->lines = reallocarray(inputLines->lines,
                    inputLines->linesAllocated, 2 * sizeof(char*));
            if (!inputLines->lines) err(2, "realloc");
            inputLines->linesAllocated *= 2;
        }

        inputLines->lines[inputLines->numLines] = readLine(file, filename);
        if (!inputLines->lines[inputLines->numLines]) {
            return;
        }
        inputLines->numLines++;
    }
}
