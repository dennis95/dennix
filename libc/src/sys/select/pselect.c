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

/* libc/src/sys/select/pselect.c
 * I/O multiplexing. (POSIX2008)
 */

#define ppoll __ppoll
#include <errno.h>
#include <poll.h>
#include <sys/select.h>

#define READ_EVENTS (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI)
#define WRITE_EVENTS (POLLOUT | POLLWRNORM | POLLWRBAND)
#define ERROR_EVENTS (POLLERR | POLLHUP)

int pselect(int nfds, fd_set* restrict readfds, fd_set* restrict writefds,
        fd_set* restrict errorfds, const struct timespec* restrict timeout,
        const sigset_t* restrict sigmask) {
    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    struct pollfd pfd[FD_SETSIZE];

    nfds_t numPfd = 0;
    for (int fd = 0; fd < nfds; fd++) {
        pfd[numPfd].fd = fd;
        pfd[numPfd].events = 0;

        if (readfds && FD_ISSET(fd, readfds)) {
            pfd[numPfd].events |= READ_EVENTS;
        }

        if (writefds && FD_ISSET(fd, writefds)) {
            pfd[numPfd].events |= WRITE_EVENTS;
        }

        if (errorfds && FD_ISSET(fd, errorfds)) {
            pfd[numPfd].events |= ERROR_EVENTS;
        }

        if (!pfd[numPfd].events) continue;

        pfd[numPfd].events |= POLLNVAL;
        numPfd++;
    }

    if (ppoll(pfd, numPfd, timeout, sigmask) < 0) return -1;

    if (readfds) {
        FD_ZERO(readfds);
    }
    if (writefds) {
        FD_ZERO(writefds);
    }
    if (errorfds) {
        FD_ZERO(errorfds);
    }

    int bitsSet = 0;
    for (nfds_t i = 0; i < numPfd; i++) {
        if (pfd[i].revents & POLLNVAL) {
            errno = EBADF;
            return -1;
        }

        if (readfds && pfd[i].revents & READ_EVENTS) {
            FD_SET(pfd[i].fd, readfds);
            bitsSet++;
        }
        if (writefds && pfd[i].revents & WRITE_EVENTS) {
            FD_SET(pfd[i].fd, writefds);
            bitsSet++;
        }
        if (errorfds && pfd[i].revents & ERROR_EVENTS) {
            FD_SET(pfd[i].fd, errorfds);
            bitsSet++;
        }
    }

    return bitsSet;
}
