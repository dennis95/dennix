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

/* libc/src/signal/signalnames.c
 * Signal names.
 */

#include <signal.h>

#define SIG(name) [SIG ## name] = #name
const char* const __signalnames[NSIG] = {
    [0] = "",
    SIG(HUP),
    SIG(INT),
    SIG(QUIT),
    SIG(ABRT),
    SIG(KILL),
    SIG(ALRM),
    SIG(TERM),

    SIG(BUS),
    SIG(CHLD),
    SIG(CONT),
    SIG(FPE),
    SIG(ILL),
    SIG(PIPE),
    SIG(SEGV),
    SIG(STOP),
    SIG(SYS),
    SIG(TRAP),
    SIG(TSTP),
    SIG(TTIN),
    SIG(TTOU),
    SIG(URG),
    SIG(USR1),
    SIG(USR2),
    SIG(WINCH),

    SIG(RTMIN),
    SIG(RTMAX),
};
