/* Copyright (c) 2018, 2021, 2022 Dennis Wölfing
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

/* sh/execute.h
 * Shell command execution.
 */

#ifndef EXECUTE_H
#define EXECUTE_H

#include "parser.h"

extern struct Function** functions;
extern size_t numFunctions;

extern unsigned long loopCounter;
extern unsigned long numBreaks;
extern unsigned long numContinues;
extern bool returning;
extern int returnStatus;

int execute(struct CompleteCommand* command);
int executeAndRead(struct CompleteCommand* command, struct StringBuffer* sb);
noreturn void executeUtility(int argc, char** arguments, char** assignments,
        size_t numAssignments);
void freeRedirections(void);
char* getExecutablePath(const char* command, bool checkExecutable);
void unsetFunction(const char* name);
void unsetFunctions(void);

#endif
