/* Copyright (c) 2022, 2023 Dennis Wölfing
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

/* utils/expr.c
 * Evaluate an expression.
 */

#include "utils.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
Grammar rules for expr:

Expr    : Or
Or      : And ('|' And)*
And     : Compar ('&' Compar)*
Compar  : Sum (('=' | '>' | '>=' | '<' | '<=' | '!=') Sum)*
Sum     : Product (('+' | '-') Product)*
Product : Match (('*' | '/' | '%') Match)*
Match   : Value (':' Value)*
Value   : '(' Or ')' | Integer | String
*/

struct Rule {
    bool (*acceptOperator)(const char* op);
    char* (*evaluate)(char* left, const char* op, char* right);
    struct Rule* nextRule;
};

static char* interpretExpression(size_t numTokens, char** tokens);
static char* interpretRule(size_t numTokens, char** tokens, size_t* used,
        struct Rule* rule);
static char* interpretValue(size_t numTokens, char** tokens, size_t* used);
static bool isNullOrZero(const char* string);

static char* xstrdup(const char* string) {
    char* copy = strdup(string);
    if (!copy) err(3, "malloc");
    return copy;
}

static bool getInteger(const char* token, long long* value) {
    char* end;
    errno = 0;
    *value = strtoll(token, &end, 10);
    return errno == 0 && !*end;
}

static bool isNullOrZero(const char* string) {
    if (strcmp(string, "") == 0) return true;

    long long value;
    return getInteger(string, &value) && value == 0;
}

int main(int argc, char* argv[]) {
    struct option longopts[] = {
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "EXPR...\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case '?':
            return 3;
        }
    }

    if (optind >= argc) errx(2, "missing operand");

    char* result = interpretExpression(argc - optind, argv + optind);
    puts(result);
    return isNullOrZero(result) ? 1 : 0;
}

static bool acceptOr(const char* op) {
    return strcmp(op, "|") == 0;
}

static char* evalOr(char* left, const char* op, char* right) {
    (void) op;
    if (isNullOrZero(left)) {
        free(left);
        return right;
    } else {
        free(right);
        return left;
    }
}

static bool acceptAnd(const char* op) {
    return strcmp(op, "&") == 0;
}

static char* evalAnd(char* left, const char* op, char* right) {
    (void) op;
    if (isNullOrZero(left) || isNullOrZero(right)) {
        free(left);
        free(right);
        return xstrdup("0");
    } else {
        free(right);
        return left;
    }
}

static bool acceptCompar(const char* op) {
    return strcmp(op, "=") == 0 ||
            strcmp(op, ">") == 0 ||
            strcmp(op, ">=") == 0 ||
            strcmp(op, "<") == 0 ||
            strcmp(op, "<=") == 0 ||
            strcmp(op, "!=") == 0;
}

static char* evalCompar(char* left, const char* op, char* right) {
    long long leftInt;
    long long rightInt;

    int comparison;
    if (getInteger(left, &leftInt) && getInteger(right, &rightInt)) {
        comparison = leftInt < rightInt ? -1 :
                leftInt > rightInt ? 1 : 0;
    } else {
        comparison = strcoll(left, right);
    }

    free(left);
    free(right);
    bool result;

    if (strcmp(op, "=") == 0) {
        result = comparison == 0;
    } else if (strcmp(op, ">") == 0) {
        result = comparison > 0;
    } else if (strcmp(op, ">=") == 0) {
        result = comparison >= 0;
    } else if (strcmp(op, "<") == 0) {
        result = comparison < 0;
    } else if (strcmp(op, "<=") == 0) {
        result = comparison <= 0;
    } else /*if (strcmp(op, "!=") == 0)*/ {
        result = comparison != 0;
    }

    return xstrdup(result ? "1" : "0");
}

static bool acceptSum(const char* op) {
    return strcmp(op, "+") == 0 || strcmp(op, "-") == 0;
}

static char* evalSum(char* left, const char* op, char* right) {
    long long leftInt;
    long long rightInt;

    if (!getInteger(left, &leftInt)) {
        errx(2, "invalid number '%s'", left);
    }
    if (!getInteger(right, &rightInt)) {
        errx(2, "invalid number '%s'", right);
    }

    free(left);
    free(right);

    long long result;
    if (*op == '+') {
        if (__builtin_add_overflow(leftInt, rightInt, &result)) {
            errx(2, "integer overflow");
        }
    } else {
        if (__builtin_sub_overflow(leftInt, rightInt, &result)) {
            errx(2, "integer overflow");
        }
    }

    char* string;
    if (asprintf(&string, "%lld", result) < 0) {
        err(3, "asprintf");
    }
    return string;
}

static bool acceptProduct(const char* op) {
    return strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0;
}

static char* evalProduct(char* left, const char* op, char* right) {
    long long leftInt;
    long long rightInt;

    if (!getInteger(left, &leftInt)) {
        errx(2, "invalid number '%s'", left);
    }
    if (!getInteger(right, &rightInt)) {
        errx(2, "invalid number '%s'", right);
    }

    free(left);
    free(right);

    long long result;
    if (*op == '*') {
        if (__builtin_mul_overflow(leftInt, rightInt, &result)) {
            errx(2, "integer overflow");
        }
    } else if (*op == '/') {
        if (rightInt == 0) errx(2, "division by zero");
        if (leftInt == LLONG_MAX && rightInt == -1) {
            errx(2, "integer overflow");
        }
        result = leftInt / rightInt;
    } else {
        if (rightInt == 0) errx(2, "division by zero");
        if (leftInt == LLONG_MAX && rightInt == -1) {
            errx(2, "integer overflow");
        }
        result = leftInt % rightInt;
    }

    char* string;
    if (asprintf(&string, "%lld", result) < 0) {
        err(3, "asprintf");
    }
    return string;
}

static bool acceptMatch(const char* op) {
    return strcmp(op, ":") == 0;
}

static char* evalMatch(char* left, const char* op, char* right) {
    (void) op;

    if (strcmp(right, ".*\\.\\(.*\\)") == 0) {
        // This is a hack to make autoconf-generated configure scripts work.
        const char* dot = strrchr(left, '.');
        return xstrdup(dot ? dot + 1 : "");
    }

    // TODO: Implement regular expression matching.
    errx(3, "regular expressions are not yet implemented");
}

static struct Rule ruleMatch = { acceptMatch, evalMatch, NULL };
static struct Rule ruleProduct = { acceptProduct, evalProduct, &ruleMatch };
static struct Rule ruleSum = { acceptSum, evalSum, &ruleProduct };
static struct Rule ruleCompar = { acceptCompar, evalCompar, &ruleSum };
static struct Rule ruleAnd = { acceptAnd, evalAnd, &ruleCompar };
static struct Rule ruleOr = { acceptOr, evalOr, &ruleAnd };

static char* interpretRule(size_t numTokens, char** tokens, size_t* used,
        struct Rule* rule) {
    if (!rule) {
        return interpretValue(numTokens, tokens, used);
    }

    size_t usedForExpr;
    char* left = interpretRule(numTokens, tokens, &usedForExpr, rule->nextRule);

    size_t tokenIndex = usedForExpr;

    while (numTokens - tokenIndex > 0) {
        char* op = tokens[tokenIndex];
        if (!rule->acceptOperator(op)) {
            *used = tokenIndex;
            return left;
        }
        tokenIndex++;

        char* right = interpretRule(numTokens - tokenIndex, tokens + tokenIndex,
            &usedForExpr, rule->nextRule);

        left = rule->evaluate(left, op, right);
        tokenIndex += usedForExpr;
    }
    *used = tokenIndex;
    return left;
}

static char* interpretExpression(size_t numTokens, char** tokens) {
    size_t used;
    char* result = interpretRule(numTokens, tokens, &used, &ruleOr);
    if (used < numTokens) {
        errx(2, "extra argument '%s'", tokens[used]);
    }
    return result;
}

static char* interpretValue(size_t numTokens, char** tokens, size_t* used) {
    if (numTokens == 0) {
        errx(2, "syntax error: unexpected end of expression");
    }

    if (numTokens > 1 && strcmp(tokens[0], "(") == 0) {
        size_t usedForExpr;
        char* expr = interpretRule(numTokens - 1, tokens + 1, &usedForExpr,
                &ruleOr);
        if (1 + usedForExpr >= numTokens ||
                strcmp(tokens[1 + usedForExpr], ")") != 0) {
            errx(2, "syntax error: expected closing parenthesis after '%s'",
                    tokens[usedForExpr]);
        }
        *used = 2 + usedForExpr;
        return expr;
    }

    *used = 1;
    return xstrdup(tokens[0]);
}
