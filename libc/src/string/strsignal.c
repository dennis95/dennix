/* Copyright (c) 2017 Dennis Wölfing
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

/* libc/src/string/strsignal.c
 * Signal names.
 */

#include <signal.h>
#include <string.h>

char* strsignal(int signum) {
    switch (signum) {
    case SIGABRT: return "Aborted";
    case SIGALRM: return "Alarm clock";
    case SIGBUS: return "Bus error";
    case SIGCHLD: return "Child exited";
    case SIGCONT: return "Continued";
    case SIGFPE: return "Arithmetic exception";
    case SIGHUP: return "Hangup";
    case SIGILL: return "Illegal instruction";
    case SIGINT: return "Interrupt";
    case SIGKILL: return "Killed";
    case SIGPIPE: return "Broken pipe";
    case SIGQUIT: return "Quit";
    case SIGSEGV: return "Segmentation fault";
    case SIGSTOP: return "Stopped (signal)";
    case SIGSYS: return "Bad system call";
    case SIGTERM: return "Terminated";
    case SIGTRAP: return "Trace/breakpoint trap";
    case SIGTSTP: return "Stopped";
    case SIGTTIN: return "Stopped (tty input)";
    case SIGTTOU: return "Stopped (tty output)";
    case SIGURG: return "Urgent I/O condition";
    case SIGUSR1: return "User defined signal 1";
    case SIGUSR2: return "User defined signal 2";
    default: return "Unknown signal";
    }
}
