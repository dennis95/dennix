/* Copyright (c) 2023 Dennis Wölfing
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

/* utils/tr.c
 * Translate characters.
 */

#include "utils.h"
#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

// TODO: Currently we only translate bytes instead of characters. The text in
//       POSIX is not fully clear on the handling of multibyte characters.

static bool delete;
static bool translate;
static bool squeeze;

static bool deleteSet[UCHAR_MAX + 1];
static unsigned char translationTable[UCHAR_MAX + 1];
static bool squeezeSet[UCHAR_MAX + 1];

enum {
    STATE_BEGIN = 0,
    STATE_CHAR,
    STATE_RANGE,
    STATE_REPEAT,
    STATE_CHARCLASS,
};

struct StateMachine {
    const char* string;
    char state;
    unsigned int rangeStart;
    unsigned char rangeEnd;
    unsigned long repetitions;
    wctype_t charclass;
};

enum {
    VALUE_CHAR,
    VALUE_CHARCLASS,
    VALUE_END
};

struct Value {
    char kind;
    unsigned char character;
    const char* charclass;
};

static void computeComplementTranslation(const char* string1,
        const char* string2);
static void computeTranslation(const char* string1, const char* string2);
static struct Value getCharacter(struct StateMachine* machine,
        bool allowRepetition);
static unsigned char handleBackslash(const char** s);
static bool mayEnd(struct StateMachine machine, struct Value value);
static void parseSet(const char* string1, bool set[static UCHAR_MAX + 1],
        bool complement, bool allowRepetition);
static void produceOutput(void);

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "delete", no_argument, 0, 'd' },
        { "squeeze-repeats", no_argument, 0, 's' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    bool complement = false;

    int c;
    while ((c = getopt_long(argc, argv, "cCds", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] STRING1 [STRING2]\n"
                    "  -c                       complement values\n"
                    "  -C                       complement characters\n"
                    "  -d, --delete             delete characters\n"
                    "  -s, --squeeze-repeats    squeeze repeated characters\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'c':
        case 'C':
            complement = true;
            break;
        case 'd':
            delete = true;
            break;
        case 's':
            squeeze = true;
            break;
        case '?':
            return 1;
        }
    }

    if (optind >= argc) errx(1, "missing string1 operand");
    if (optind + 2 < argc) errx(1, "extra operand");

    const char* string1 = argv[optind];
    const char* string2 = argv[optind + 1];

    const char* squeezeString = NULL;

    if (delete) {
        if (squeeze) {
            if (!string2) errx(1, "missing string2 operand");
            squeezeString = string2;
        } else {
            if (string2) errx(1, "extra operand");
        }
    } else {
        if (squeeze) {
            if (!string2) {
                squeezeString = string1;
            } else {
                translate = true;
                squeezeString = string2;
            }
        } else {
            if (!string2) errx(1, "missing string2 operand");
            translate = true;
        }
    }

    if (translate) {
        if (complement) {
            computeComplementTranslation(string1, string2);
        } else {
            computeTranslation(string1, string2);
        }
    }

    if (delete) {
        parseSet(string1, deleteSet, complement, false);
    }
    if (squeeze) {
        bool squeezeIsFirst = squeezeString == string1;
        parseSet(squeezeString, squeezeSet, complement && squeezeIsFirst,
                !squeezeIsFirst);
    }

    produceOutput();
}

static void computeComplementTranslation(const char* string1,
        const char* string2) {
    for (unsigned int i = 0; i <= UCHAR_MAX; i++) {
        translationTable[i] = i;
    }

    bool containedInFirstString[UCHAR_MAX + 1] = {0};

    struct StateMachine machine1 = {0};
    machine1.string = string1;

    while (true) {
        struct Value from = getCharacter(&machine1, false);

        if (from.kind == VALUE_CHAR) {
            containedInFirstString[from.character] = true;
        } else if (from.kind == VALUE_CHARCLASS) {
            machine1.state = STATE_CHARCLASS;
            machine1.charclass = wctype(from.charclass);
            machine1.rangeStart = 0;
        } else if (from.kind == VALUE_END) {
            break;
        }
    }

    struct StateMachine machine2 = {0};
    machine2.string = string2;
    unsigned int c = 0;

    while (true) {
        while (c <= UCHAR_MAX && containedInFirstString[c]) {
            c++;
        }

        struct Value from;
        if (c > UCHAR_MAX) {
            from.kind = VALUE_END;
        } else {
            from.kind = VALUE_CHAR;
            from.character = c;
        }

        struct Value to = getCharacter(&machine2, true);

        if (from.kind == VALUE_END || to.kind == VALUE_END) {
            if (from.kind != VALUE_END) {
                errx(1, "string1 is too long");
            }
            if (!mayEnd(machine2, to)) {
                errx(1, "string2 is too long");
            }
            return;
        }

        if (to.kind == VALUE_CHARCLASS) {
            errx(1, "character class not valid in string2");
        }

        translationTable[from.character] = to.character;

        c++;
    }
}

static void computeTranslation(const char* string1, const char* string2) {
    for (unsigned int i = 0; i <= UCHAR_MAX; i++) {
        translationTable[i] = i;
    }

    struct StateMachine machine1 = {0};
    machine1.string = string1;
    struct StateMachine machine2 = {0};
    machine2.string = string2;

    while (true) {
        struct Value from = getCharacter(&machine1, false);

        struct Value to = getCharacter(&machine2, true);

restartCharProcessing:
        if (from.kind == VALUE_END || to.kind == VALUE_END) {
            if (from.kind != VALUE_END) {
                errx(1, "string1 is too long");
            }
            if (!mayEnd(machine2, to)) {
                errx(1, "string2 is too long");
            }
            return;
        }

        if (from.kind == VALUE_CHARCLASS && to.kind == VALUE_CHARCLASS) {
            if (strcmp(from.charclass, "upper") == 0 &&
                    strcmp(to.charclass, "lower") == 0) {
                for (unsigned int i = 0; i <= UCHAR_MAX; i++) {
                    if (isupper(i)) {
                        translationTable[i] = tolower(i);
                    }
                }
            } else if (strcmp(from.charclass, "lower") == 0 &&
                    strcmp(to.charclass, "upper") == 0) {
                for (unsigned int i = 0; i <= UCHAR_MAX; i++) {
                    if (islower(i)) {
                        translationTable[i] = toupper(i);
                    }
                }
            } else {
                errx(1, "invalid character class conversion");
            }
        } else if (to.kind == VALUE_CHARCLASS) {
            errx(1, "character class in string2 does not have corresponding "
                    "character class in string1");
        } else if (from.kind == VALUE_CHARCLASS) {
            machine1.state = STATE_CHARCLASS;
            machine1.charclass = wctype(from.charclass);
            machine1.rangeStart = 0;
            from = getCharacter(&machine1, false);
            goto restartCharProcessing;
        } else {
            translationTable[from.character] = to.character;
        }
    }
}

static struct Value getCharacter(struct StateMachine* machine,
        bool allowRepetition) {
    struct Value result;

    if (machine->state == STATE_RANGE) {
        if (machine->rangeStart > machine->rangeEnd) {
            machine->state = STATE_BEGIN;
        } else {
            result.kind = VALUE_CHAR;
            result.character = machine->rangeStart++;
            return result;
        }
    } else if (machine->state == STATE_REPEAT) {
        result.kind = VALUE_CHAR;
        result.character = machine->rangeStart;
        if (machine->repetitions == 1) {
            machine->state = STATE_BEGIN;
        } else if (machine->repetitions > 1) {
            machine->repetitions--;
        }
        return result;
    } else if (machine->state == STATE_CHARCLASS) {
        while (machine->rangeStart <= UCHAR_MAX) {
            if (iswctype(btowc(machine->rangeStart), machine->charclass)) {
                result.kind = VALUE_CHAR;
                result.character = machine->rangeStart++;
                return result;
            }
            machine->rangeStart++;
        }
        machine->state = STATE_BEGIN;
    }

    unsigned char c;
    if (*machine->string == '\0') {
        result.kind = VALUE_END;
        return result;
    } if (*machine->string == '\\') {
        c = handleBackslash(&machine->string);
        machine->state = STATE_CHAR;
        machine->rangeStart = c;
    } else if (*machine->string == '-') {
        if (machine->state != STATE_CHAR || machine->string[1] == '\0') {
            c = '-';
            machine->state = STATE_CHAR;
            machine->rangeStart = c;
            machine->string++;
        } else {
            machine->string++;
            if (*machine->string == '\\') {
                machine->rangeEnd = handleBackslash(&machine->string);
            } else {
                machine->rangeEnd = *machine->string;
                machine->string++;
            }
            machine->state = STATE_RANGE;
            if (machine->rangeStart > machine->rangeEnd) {
                errx(1, "invalid range '%c-%c'", machine->rangeStart,
                        machine->rangeEnd);
            }
            machine->rangeStart++;
            return getCharacter(machine, allowRepetition);
        }
    } else if (*machine->string == '[') {
        machine->string++;
        if (*machine->string == ':') {
            const char* begin = machine->string + 1;
            const char* end = strstr(begin, ":]");
            if (end) {
                result.kind = VALUE_CHARCLASS;
                result.charclass = strndup(begin, end - begin);
                if (!result.charclass) err(1, "malloc");
                machine->state = STATE_BEGIN;
                machine->string = end + 2;
                return result;
            }

            c = '[';
            machine->state = STATE_CHAR;
            machine->rangeStart = c;
        } else if (*machine->string == '=') {
            if (machine->string[1] != '\0' && machine->string[2] == '=' &&
                    machine->string[3] == ']') {
                c = machine->string[1];
                machine->string += 4;
                machine->state = STATE_BEGIN;
            } else {
                c = '[';
                machine->state = STATE_CHAR;
                machine->rangeStart = c;
            }
        } else {
            const char* string = machine->string;
            if (*string == '\\') {
                c = handleBackslash(&string);
            } else {
                c = *string;
                string++;
            }
            if (*string == '*') {
                char* end;
                unsigned long repetitions = strtoul(string + 1, &end,
                        string[1] == '0' ? 8 : 10);
                if (*end == ']') {
                    if (!allowRepetition) {
                        errx(1, "repetitions are not allowed in string1");
                    }

                    if (repetitions == 0) {
                        machine->state = STATE_REPEAT;
                        machine->rangeStart = c;
                        machine->repetitions = 0;
                    } else if (repetitions == 1) {
                        machine->state = STATE_BEGIN;
                    } else {
                        machine->state = STATE_REPEAT;
                        machine->rangeStart = c;
                        machine->repetitions = repetitions - 1;
                    }

                    machine->string = end + 1;

                    if (repetitions == 0 && *machine->string != '\0') {
                        // TODO: To properly support unbounded repetitions
                        // anywhere in the string we would need to look ahead.
                        errx(1, "unimplemented: unbounded repetitions are only "
                                "supported at the end of the string");
                    }

                    result.kind = VALUE_CHAR;
                    result.character = c;
                    return result;
                }
            }

            c = '[';
            machine->state = STATE_CHAR;
            machine->rangeStart = c;
        }
    } else {
        c = *machine->string++;
        machine->state = STATE_CHAR;
        machine->rangeStart = c;
    }

    result.kind = VALUE_CHAR;
    result.character = c;

    return result;
}

static unsigned char handleBackslash(const char** s) {
    (*s)++;
    unsigned char c;
    switch (**s) {
    case '\\': c = '\\'; break;
    case 'a': c = '\a'; break;
    case 'b': c = '\b'; break;
    case 'f': c = '\f'; break;
    case 'n': c = '\n'; break;
    case 'r': c = '\r'; break;
    case 't': c = '\t'; break;
    case 'v': c = '\v'; break;
    default:
        if (**s >= '0' && **s <= '7') {
            c = **s - '0';
            (*s)++;
            for (int i = 1; i < 3; i++) {
                if (**s < '0' || **s > '7') break;
                c = c * 8 + (**s - '0');
                (*s)++;
            }
            return c;
        } else {
            errx(1, "invalid escape sequence '\\%c'", **s);
        }
    }

    (*s)++;
    return c;
}

static bool mayEnd(struct StateMachine machine, struct Value value) {
    return value.kind == VALUE_END ||
            (machine.state == STATE_REPEAT && machine.repetitions == 0);
}

static void parseSet(const char* string1, bool set[static UCHAR_MAX + 1],
        bool complement, bool allowRepetition) {
    bool containedInString[UCHAR_MAX + 1] = {0};

    struct StateMachine machine = {0};
    machine.string = string1;

    while (true) {
        struct Value from = getCharacter(&machine, allowRepetition);

        if (from.kind == VALUE_CHARCLASS) {
            machine.state = STATE_CHARCLASS;
            machine.charclass = wctype(from.charclass);
            machine.rangeStart = 0;
        } else if (from.kind == VALUE_CHAR) {
            containedInString[from.character] = true;
        }

        if (mayEnd(machine, from)) {
            break;
        }
    }

    for (unsigned int i = 0; i <= UCHAR_MAX; i++) {
        set[i] = complement ^ containedInString[i];
    }
}

static void produceOutput(void) {
    unsigned char buffer[4096];
    unsigned int lastCharacter = UCHAR_MAX + 1;

    while (!feof(stdin)) {
        size_t byteCount = fread(buffer, 1, sizeof(buffer), stdin);
        if (ferror(stdin)) {
            err(1, "failed to read from stdin");
        }

        if (delete) {
            size_t writeIndex = 0;
            for (size_t readIndex = 0; readIndex < byteCount; readIndex++) {
                if (deleteSet[buffer[readIndex]]) {
                    // This char will be omitted.
                } else {
                    buffer[writeIndex] = buffer[readIndex];
                    writeIndex++;
                }
            }

            byteCount = writeIndex;
        }

        if (translate) {
            for (size_t i = 0; i < byteCount; i++) {
                buffer[i] = translationTable[buffer[i]];
            }
        }

        if (squeeze) {
            size_t writeIndex = 0;
            for (size_t readIndex = 0; readIndex < byteCount; readIndex++) {
                if (buffer[readIndex] == lastCharacter &&
                        squeezeSet[lastCharacter]) {
                    // This char will be omitted.
                } else {
                    lastCharacter = buffer[readIndex];
                    buffer[writeIndex] = buffer[readIndex];
                    writeIndex++;
                }
            }

            byteCount = writeIndex;
        }

        if (fwrite(buffer, 1, byteCount, stdout) != byteCount) {
            err(1, "failed to write to stdout");
        }
    }
}
