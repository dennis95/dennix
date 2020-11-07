/* Copyright (c) 2020 Dennis Wölfing
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

/* utils/test.c
 * Evaluate expression.
 */

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static bool test(int count, const char* args[]);
static long toInteger(const char* operand);
static bool unary(const char* operator, const char* operand);
static bool isBinary(const char* operator);
static bool binary(const char* operand1, const char* operator,
        const char* operand2);

int main(int argc, char* argv[]) {
    char* base = basename(argv[0]);
    if (strcmp(base, "[") == 0) {
        // The last argument must be ]
        if (strcmp(argv[argc - 1], "]") != 0) {
            errx(2, "missing ']'");
        }
        argc--;
    }

    return test(argc - 1, (const char**) argv + 1) ? 0 : 1;
}

static bool test(int count, const char* args[]) {
    if (count == 0) {
        return false;
    } else if (count == 1) {
        return *args[0];
    } else if (count == 2) {
        if (strcmp(args[0], "!") == 0) {
            return !*args[1];
        } else {
            return unary(args[0], args[1]);
        }
    } else if (count == 3) {
        if (isBinary(args[1])) {
            return binary(args[0], args[1], args[2]);
        } else if (strcmp(args[0], "!") == 0) {
            return !test(2, args + 1);
        }
    } else if (count == 4) {
        if (strcmp(args[0], "!") == 0) {
            return !test(3, args + 1);
        }
    }

    errx(2, "too many operands");
}

static long toInteger(const char* operand) {
    char* end;
    errno = 0;
    long result = strtol(operand, &end, 10);
    if (errno || *end) errx(2, "invalid integer expression '%s'", operand);
    return result;
}

static bool unary(const char* operator, const char* operand) {
    struct stat st;
    if (strcmp(operator, "-b") == 0) {
        if (stat(operand, &st) < 0) return false;
        return S_ISBLK(st.st_mode);
    } else if (strcmp(operator, "-c") == 0) {
        if (stat(operand, &st) < 0) return false;
        return S_ISCHR(st.st_mode);
    } else if (strcmp(operator, "-d") == 0) {
        if (stat(operand, &st) < 0) return false;
        return S_ISDIR(st.st_mode);
    } else if (strcmp(operator, "-e") == 0) {
        return access(operand, F_OK) == 0;
    } else if (strcmp(operator, "-f") == 0) {
        if (stat(operand, &st) < 0) return false;
        return S_ISREG(st.st_mode);
    } else if (strcmp(operator, "-g") == 0) {
        if (stat(operand, &st) < 0) return false;
        return st.st_mode & S_ISGID;
    } else if (strcmp(operator, "-h") == 0 || strcmp(operator, "-L") == 0) {
        if (lstat(operand, &st) < 0) return false;
        return S_ISLNK(st.st_mode);
    } else if (strcmp(operator, "-n") == 0) {
        return *operand;
    } else if (strcmp(operator, "-p") == 0) {
        if (stat(operand, &st) < 0) return false;
        return S_ISFIFO(st.st_mode);
    } else if (strcmp(operator, "-r") == 0) {
        return access(operand, R_OK) == 0;
    } else if (strcmp(operator, "-S") == 0) {
        if (stat(operand, &st) < 0) return false;
        return S_ISSOCK(st.st_mode);
    } else if (strcmp(operator, "-s") == 0) {
        if (stat(operand, &st) < 0) return false;
        return st.st_size > 0;
    } else if (strcmp(operator, "-t") == 0) {
        long fd = toInteger(operand);
        if (fd < 0 || fd > INT_MAX) return false;
        return isatty(fd);
    } else if (strcmp(operator, "-u") == 0) {
        if (stat(operand, &st) < 0) return false;
        return st.st_mode & S_ISUID;
    } else if (strcmp(operator, "-w") == 0) {
        return access(operand, W_OK) == 0;
    } else if (strcmp(operator, "-x") == 0) {
        return access(operand, X_OK) == 0;
    } else if (strcmp(operator, "-z") == 0) {
        return !*operand;
    } else {
        errx(2, "invalid unary operator '%s'", operator);
    }
}

static bool isBinary(const char* operator) {
    static const char* binaries[] =
            { "=", "!=", "-eq", "-ne", "-gt", "-ge", "-lt", "-le", NULL };
    for (size_t i = 0; binaries[i]; i++) {
        if (strcmp(operator, binaries[i]) == 0) return true;
    }
    return false;
}

static bool binary(const char* operand1, const char* operator,
        const char* operand2) {
    if (strcmp(operator, "=") == 0) {
        return strcmp(operand1, operand2) == 0;
    } else if (strcmp(operator, "!=") == 0) {
        return strcmp(operand1, operand2) != 0;
    } else if (strcmp(operator, "-eq") == 0) {
        return toInteger(operand1) == toInteger(operand2);
    } else if (strcmp(operator, "-ne") == 0) {
        return toInteger(operand1) != toInteger(operand2);
    } else if (strcmp(operator, "-gt") == 0) {
        return toInteger(operand1) > toInteger(operand2);
    } else if (strcmp(operator, "-ge") == 0) {
        return toInteger(operand1) >= toInteger(operand2);
    } else if (strcmp(operator, "-lt") == 0) {
        return toInteger(operand1) < toInteger(operand2);
    } else if (strcmp(operator, "-le") == 0) {
        return toInteger(operand1) <= toInteger(operand2);
    } else {
        errx(2, "invalid binary operator '%s'", operator);
    }
}
