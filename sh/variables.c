/* Copyright (c) 2019 Dennis Wölfing
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

/* sh/variables.c
 * Shell variables.
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh.h"
#include "variables.h"

extern char** environ;

char** arguments;
int numArguments;

struct ShellVar {
    char* name;
    char* value;
};

// This buffer is large enough to contain any $- value or any 32 bit integer.
static char buffer[15];
static struct ShellVar* variables;
static size_t variablesAllocated;

const char* getVariable(const char* name) {
    if (isdigit(*name)) {
        char* end;
        long i = strtol(name, &end, 10);
        if (i < 0 || i > numArguments || *end) {
            return NULL;
        }
        return arguments[i];
    }

    if (strcmp(name, "#") == 0) {
        snprintf(buffer, sizeof(buffer), "%d", numArguments);
        return buffer;
    } else if (strcmp(name, "?") == 0) {
        snprintf(buffer, sizeof(buffer), "%d", lastStatus);
        return buffer;
    } else if (strcmp(name, "-") == 0) {
        char* flags = buffer;
        if (shellOptions.allexport) *flags++ = 'a';
        if (shellOptions.notify) *flags++ = 'b';
        if (shellOptions.command) *flags++ = 'c';
        if (shellOptions.noclobber) *flags++ = 'C';
        if (shellOptions.errexit) *flags++ = 'e';
        if (shellOptions.noglob) *flags++ = 'f';
        if (shellOptions.hashall) *flags++ = 'h';
        if (shellOptions.interactive) *flags++ = 'i';
        if (shellOptions.monitor) *flags++ = 'm';
        if (shellOptions.noexec) *flags++ = 'n';
        if (shellOptions.stdInput) *flags++ = 's';
        if (shellOptions.nounset) *flags++ = 'u';
        if (shellOptions.verbose) *flags++ = 'v';
        if (shellOptions.xtrace) *flags++ = 'x';
        *flags = '\0';
        return buffer;
    }

    for (size_t i = 0; i < variablesAllocated; i++) {
        struct ShellVar* var = &variables[i];
        if (strcmp(name, var->name) == 0) {
            if (var->value) {
                return var->value;
            } else {
                return getenv(name);
            }
        }
    }

    return NULL;
}

void initializeVariables(void) {
    // Free the old variables in case we reset the old vars.
    for (size_t i = 0; i < variablesAllocated; i++) {
        free(variables[i].name);
        free(variables[i].value);
    }

    variablesAllocated = 0;

    for (const char** envp = (const char**) environ; *envp; envp++) {
        size_t nameLength = strcspn(*envp, "=");

        variables = reallocarray(variables,
                variablesAllocated + 1, sizeof(struct ShellVar));
        if (!variables) err(1, "malloc");
        variables[variablesAllocated].name = strndup(*envp, nameLength);
        if (!variables[variablesAllocated].name) err(1, "strdup");
        variables[variablesAllocated].value = NULL;
        variablesAllocated++;
    }
}

void setVariable(const char* name, const char* value, bool export) {
    for (size_t i = 0; i < variablesAllocated; i++) {
        struct ShellVar* var = &variables[i];
        if (strcmp(name, var->name) == 0) {
            if (!export && var->value) {
                free(var->value);
                var->value = strdup(value);
                if (!var->value) err(1, "strdup");
            } else {
                var->value = NULL;
                if (setenv(name, value, 1) < 0) {
                    err(1, "setenv");
                }
            }
            return;
        }
    }

    variables = reallocarray(variables, variablesAllocated + 1,
            sizeof(struct ShellVar));
    if (!variables) err(1, "malloc");
    variables[variablesAllocated].name = strdup(name);
    variables[variablesAllocated].value = export ? NULL : strdup(value);
    if (!variables[variablesAllocated].name ||
            (!export && !variables[variablesAllocated].value)) {
        err(1, "strdup");
    }
    variablesAllocated++;

    if (export) {
        if (setenv(name, value, 1) < 0) err(1, "setenv");
    }
}

void unsetVariable(const char* name) {
    for (size_t i = 0; i < variablesAllocated; i++) {
        if (strcmp(name, variables[i].name) == 0) {
            if (!variables[i].value) {
                unsetenv(name);
            }
            free(variables[i].name);
            free(variables[i].value);
            variables[i] = variables[variablesAllocated - 1];
            variablesAllocated--;
            return;
        }
    }
}
