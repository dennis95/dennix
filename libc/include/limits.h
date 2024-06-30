/* Copyright (c) 2019, 2020, 2022, 2023, 2024 Dennis Wölfing
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

/* libc/include/limits.h
 * Implementation limits.
 */

#ifndef _LIMITS_H
#define _LIMITS_H

#include <sys/cdefs.h>

/* <limits.h> is provided by both GCC and libc. Both headers need to include
   each other so that everything will work no matter which order they occur in
   the include path. */
#define _LIBC_LIMITS_H_

#ifndef _GCC_LIMITS_H_
#  include_next <limits.h>
#endif

/* GCC <limits.h> defines this to 1 which is wrong for UTF-8. */
#undef MB_LEN_MAX
#define MB_LEN_MAX 4

#if __USE_DENNIX || __USE_POSIX
#  include <dennix/limits.h>

#  define ATEXIT_MAX 32
#  define HOST_NAME_MAX 255
#  define PTHREAD_DESTRUCTOR_ITERATIONS 4
#  define PTHREAD_KEYS_MAX 128

#  define LONG_BIT (__SIZEOF_LONG__ * CHAR_BIT)
#  define SSIZE_MAX LONG_MAX
#  define WORD_BIT (__SIZEOF_INT__ * CHAR_BIT)

/* The following values are defined by POSIX and are not implementation
   specific. */

#  define _POSIX_CLOCKRES_MIN 20000000

#  define _POSIX_AIO_LISTIO_MAX 2
#  define _POSIX_AIO_MAX 1
#  define _POSIX_ARG_MAX 4096
#  define _POSIX_CHILD_MAX 25
#  define _POSIX_DELAYTIMER_MAX 32
#  define _POSIX_HOST_NAME_MAX 255
#  define _POSIX_LINK_MAX 8
#  define _POSIX_LOGIN_NAME_MAX 9
#  define _POSIX_MAX_CANON 255
#  define _POSIX_MAX_INPUT 255
#  define _POSIX_NAME_MAX 14
#  define _POSIX_NGROUPS_MAX 8
#  define _POSIX_OPEN_MAX 20
#  define _POSIX_PATH_MAX 256
#  define _POSIX_PIPE_BUF 512
#  define _POSIX_RE_DUP_MAX 255
#  define _POSIX_RTSIG_MAX 8
#  define _POSIX_SEM_NSEMS_MAX 256
#  define _POSIX_SEM_VALUE_MAX 32767
#  define _POSIX_SIGQUEUE_MAX 32
#  define _POSIX_SSIZE_MAX 32767
#  define _POSIX_STREAM_MAX 8
#  define _POSIX_SYMLINK_MAX 255
#  define _POSIX_SYMLOOP_MAX 8
#  define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4
#  define _POSIX_THREAD_KEYS_MAX 128
#  define _POSIX_THREAD_THREADS_MAX 64
#  define _POSIX_TIMER_MAX 32
#  define _POSIX_TTY_NAME_MAX 9
#  define _POSIX_TZNAME_MAX 6
#  define _POSIX2_BC_BASE_MAX 99
#  define _POSIX2_BC_DIM_MAX 2048
#  define _POSIX2_BC_SCALE_MAX 99
#  define _POSIX2_BC_STRING_MAX 1000
#  define _POSIX2_CHARCLASS_NAME_MAX 14
#  define _POSIX2_COLL_WEIGHTS_MAX 2
#  define _POSIX2_EXPR_NEST_MAX 32
#  define _POSIX2_LINE_MAX 2048
#  define _POSIX2_RE_DUP_MAX 255
#endif

#if __USE_DENNIX || __USE_POSIX >= 202405L
#  define GETENTROPY_MAX _GETENTROPY_MAX
#  define NSIG_MAX _NSIG_MAX
#endif

#endif
