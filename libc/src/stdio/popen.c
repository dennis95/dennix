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

/* libc/src/stdio/popen.c
 * Open a pipe to another process. (POSIX2008)
 */

#define pipe2 __pipe2
#include "FILE.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

struct PipeFile {
    FILE file;
    pid_t pid;
    struct PipeFile* prev;
    struct PipeFile* next;
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static struct PipeFile* firstPipeFile;

FILE* popen(const char* command, const char* mode) {
    if ((mode[0] != 'r' && mode[0] != 'w') ||
            (mode[1] != '\0' && mode[1] != 'e') ||
            (mode[1] == 'e' && mode[2] != '\0')) {
        errno = EINVAL;
        return NULL;
    }


    struct PipeFile* pipeFile = malloc(sizeof(struct PipeFile));
    if (!pipeFile) return NULL;
    FILE* file = &pipeFile->file;
    file->buffer = malloc(BUFSIZ);
    if (!file->buffer) {
        free(pipeFile);
        return NULL;
    }

    pthread_mutex_lock(&mutex);

    int fd[2];
    if (pipe2(fd, mode[1] == 'e' ? O_CLOEXEC : 0) < 0) {
        pthread_mutex_unlock(&mutex);
        free(file->buffer);
        free(pipeFile);
        return NULL;
    }

    int parentTarget = mode[0] == 'w';
    int childTarget = !parentTarget;

    pid_t pid = fork();
    if (pid < 0) {
        pthread_mutex_unlock(&mutex);
        free(file->buffer);
        free(pipeFile);
        close(fd[0]);
        close(fd[1]);
        return NULL;
    } else if (pid == 0) {
        for (struct PipeFile* f = firstPipeFile; f; f = f->next) {
            close(f->file.fd);
        }

        close(fd[parentTarget]);

        if (fd[childTarget] != childTarget) {
            if (dup2(fd[childTarget], childTarget) < 0) _Exit(127);
            close(fd[childTarget]);
        } else if (mode[1] == 'e') {
            int flags = fcntl(childTarget, F_GETFD);
            flags &= ~FD_CLOEXEC;
            if (fcntl(childTarget, F_SETFD, flags) < 0) _Exit(127);
        }

        execl("/bin/sh", "sh", "-c", "--", command, NULL);
        _Exit(127);
    }

    close(fd[childTarget]);
    file->fd = fd[parentTarget];

    file->flags = FILE_FLAG_BUFFERED;
    file->bufferSize = BUFSIZ;
    file->readPosition = UNGET_BYTES;
    file->readEnd = UNGET_BYTES;
    file->writePosition = 0;

    file->mutex = (__mutex_t) _MUTEX_INIT(_MUTEX_RECURSIVE);
    file->read = __file_read;
    file->write = __file_write;
    file->seek = __file_seek;

    pthread_mutex_lock(&__fileListMutex);
    file->prev = NULL;
    file->next = __firstFile;
    if (file->next) {
        file->next->prev = file;
    }
    __firstFile = file;
    pthread_mutex_unlock(&__fileListMutex);

    pipeFile->pid = pid;
    pipeFile->prev = NULL;
    pipeFile->next = firstPipeFile;
    if (pipeFile->next) {
        pipeFile->next->prev = pipeFile;
    }
    firstPipeFile = pipeFile;

    pthread_mutex_unlock(&mutex);
    return file;
}

int pclose(FILE* file) {
    struct PipeFile* pipeFile = (struct PipeFile*) file;

    pthread_mutex_lock(&mutex);
    if (pipeFile->next) {
        pipeFile->next->prev = pipeFile->prev;
    }

    if (pipeFile->prev) {
        pipeFile->prev->next = pipeFile->next;
    } else {
        firstPipeFile = pipeFile->next;
    }
    pid_t pid = pipeFile->pid;
    fclose(file);
    pthread_mutex_unlock(&mutex);

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return status;
}
