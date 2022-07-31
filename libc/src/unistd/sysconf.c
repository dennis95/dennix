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

/* libc/src/unistd/sysconf.c
 * Query system configuration variables. (POSIX2008)
 */

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

long sysconf(int name) {
    switch (name) {
    case __SC_UNLIMITED: return -1;

    case _SC_ATEXIT_MAX: return ATEXIT_MAX;
    case _SC_CLK_TICK: return CLOCKS_PER_SEC;
    case _SC_HOST_NAME_MAX: return HOST_NAME_MAX;
    case _SC_NSIG: return NSIG;
    case _SC_PAGESIZE: return PAGESIZE;
    case _SC_SYMLOOP_MAX: return SYMLOOP_MAX;
    case _SC_TTY_NAME_MAX: return TTY_NAME_MAX;

    case _SC_ADVISORY_INFO: return -1;
    case _SC_ASYNCHRONOUS_IO: return -1;
    case _SC_BARRIERS: return -1;
    case _SC_CLOCK_SELECTION: return _POSIX_CLOCK_SELECTION;
    case _SC_CPUTIME: return _POSIX_CPUTIME;
    case _SC_FSYNC: return _POSIX_FSYNC;
    case _SC_IPV6: return -1;
    case _SC_JOB_CONTROL: return _POSIX_JOB_CONTROL;
    case _SC_MAPPED_FILES: return _POSIX_MAPPED_FILES;
    case _SC_MEMLOCK: return -1;
    case _SC_MEMLOCK_RANGE: return -1;
    case _SC_MEMORY_PROTECTION: return -1;
    case _SC_MESSAGE_PASSING: return -1;
    case _SC_MONOTONIC_CLOCK: return _POSIX_MONOTONIC_CLOCK;
    case _SC_PRIORITIZED_IO: return -1;
    case _SC_PRIORITY_SCHEDULING: return -1;
    case _SC_RAW_SOCKETS: return -1;
    case _SC_READER_WRITER_LOCKS: return -1;
    case _SC_REALTIME_SIGNALS: return -1;
    case _SC_REGEXP: return -1;
    case _SC_SAVED_IDS: return -1;
    case _SC_SEMAPHORES: return -1;
    case _SC_SHARED_MEMORY_OBJECTS: return -1;
    case _SC_SHELL: return _POSIX_SHELL;
    case _SC_SPAWN: return -1;
    case _SC_SPIN_LOCKS: return -1;
    case _SC_SPORADIC_SERVER: return -1;
    case _SC_SYNCHRONIZED_IO: return _POSIX_SYNCHRONIZED_IO;
    case _SC_THREAD_ATTR_STACKADDR: return -1;
    case _SC_THREAD_ATTR_STACKSIZE: return -1;
    case _SC_THREAD_CPUTIME: return -1;
    case _SC_THREAD_PRIO_INHERIT: return -1;
    case _SC_THREAD_PRIO_PROTECT: return -1;
    case _SC_THREAD_PRIORITY_SCHEDULING: return -1;
    case _SC_THREAD_PROCESS_SHARED: return -1;
    case _SC_THREAD_ROBUST_PRIO_INHERIT: return -1;
    case _SC_THREAD_ROBUST_PRIO_PROTECT: return -1;
    case _SC_THREAD_SAFE_FUNCTIONS: return _POSIX_THREAD_SAFE_FUNCTIONS;
    case _SC_THREAD_SPORADIC_SERVER: return -1;
    case _SC_THREADS: return _POSIX_THREADS;
    case _SC_TIMEOUTS: return -1;
    case _SC_TIMERS: return _POSIX_TIMERS;
    case _SC_TYPED_MEMORY_OBJECTS: return -1;
    case _SC_VERSION: return _POSIX_VERSION;
    case _SC_V6_ILP32_OFF32: return -1;
    case _SC_V6_ILP32_OFFBIG: return -1;
    case _SC_V6_LP64_OFF64: return -1;
    case _SC_V6_LPBIG_OFFBIG: return -1;
    case _SC_V7_ILP32_OFF32: return -1;
    case _SC_V7_ILP32_OFFBIG: return -1;
    case _SC_V7_LP64_OFF64: return -1;
    case _SC_V7_LPBIG_OFFBIG: return -1;
    case _SC_2_C_BIND: return _POSIX2_C_BIND;
    case _SC_2_C_DEV: return -1;
    case _SC_2_CHAR_TERM: return -1;
    case _SC_2_FORT_RUN: return -1;
    case _SC_2_LOCALEDEF: return -1;
    case _SC_2_SW_DEV: return -1;
    case _SC_2_UPE: return -1;
    case _SC_2_VERSION: return _POSIX2_VERSION;
    case _SC_XOPEN_CRYPT: return -1;
    case _SC_XOPEN_ENH_I18N: return -1;
    case _SC_XOPEN_REALTIME: return -1;
    case _SC_XOPEN_REALTIME_THREADS: return -1;
    case _SC_XOPEN_SHM: return -1;
    case _SC_XOPEN_UNIX: return -1;
    case _SC_XOPEN_UUCP: return -1;
    case _SC_XOPEN_VERSION: return -1;

    default:
        errno = EINVAL;
        return -1;
    }
}
