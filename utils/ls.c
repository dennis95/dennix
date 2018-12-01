/* Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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

/* utils/ls.c
 * Lists directory contents.
 */

#include "utils.h"
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

struct DirEntry {
    char* name;
    struct stat stat;
};

struct DirListing {
    struct DirEntry** entries;
    size_t numEntries;
    size_t allocated;
};

static void addEntry(struct DirListing* listing, int dirFd, const char* name);
static void freeEntries(struct DirListing* listing);
static void getColor(mode_t mode, const char** pre, const char** post);
static bool getDirectoryEntries(struct DirListing* listing, const char* path);
static void list(struct DirListing* listing);
static void printMode(mode_t mode);

static struct timespec getATime(struct DirEntry* entry);
static struct timespec getCTime(struct DirEntry* entry);
static struct timespec getMTime(struct DirEntry* entry);
static void outputColumns(struct DirEntry** entries, size_t numEntries);
static void outputLong(struct DirEntry** entries, size_t numEntries);
static void outputOneline(struct DirEntry** entries, size_t numEntries);
static bool selectAll(const char* name);
static bool selectAlmostAll(const char* name);
static bool selectDefault(const char* name);
static int sortByName(struct DirEntry** entry1, struct DirEntry** entry2);
static int sortBySize(struct DirEntry** entry1, struct DirEntry** entry2);
static int sortByTime(struct DirEntry** entry1, struct DirEntry** entry2);

static struct timespec (*getTime)(struct DirEntry*) = getMTime;
static void (*output)(struct DirEntry**, size_t) = outputOneline;
static bool (*selectEntry)(const char*) = selectDefault;
static int (*sort)(struct DirEntry**, struct DirEntry**) = sortByName;

static bool colors = false;
static bool noGroup = false;
static bool noOwner = false;
static bool numericUidGid = false;
static bool printInode = false;
static const time_t sixMonths = 182 * 24 * 60 * 60;
static bool success = true;
static bool unsorted = false;

int main(int argc, char* argv[]) {
    if (isatty(1)) {
        output = outputColumns;
        colors = true;
    }

    struct option longopts[] = {
        { "almost-all", no_argument, 0, 'A' },
        { "all", no_argument, 0, 'a' },
        { "inode", no_argument, 0, 'i' },
        { "numeric-uid-gid", no_argument, 0, 'n' },
        { "help", no_argument, 0, 0 },
        { "version", no_argument, 0, 1 },
        { 0, 0, 0, 0 }
    };
    // TODO: Implement more options.
    const char* shortopts = "ACSacfgilnotu1";

    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (c) {
        case 0:
            return help(argv[0], "[OPTIONS] [FILE...]\n"
                    "  -A, --almost-all         list hidden files\n"
                    "  -C                       column output\n"
                    "  -S                       sort by size\n"
                    "  -a, --all                list all files\n"
                    "  -c                       use status change time\n"
                    "  -f                       unsorted output\n"
                    "  -g                       long output without owner\n"
                    "  -i, --inode              write inode number\n"
                    "  -l                       long output\n"
                    "  -n, --numeric-uid-gid    write numeric uid/gid\n"
                    "  -o                       long output without group\n"
                    "  -t                       sort by modification time\n"
                    "  -u                       use access time\n"
                    "  -1                       oneline output\n"
                    "      --help               display this help\n"
                    "      --version            display version info");
        case 1:
            return version(argv[0]);
        case 'A':
            if (selectEntry != selectAll) {
                selectEntry = selectAlmostAll;
            }
            break;
        case 'C':
            output = outputColumns;
            break;
        case 'S':
            sort = sortBySize;
            break;
        case 'a':
            selectEntry = selectAll;
            break;
        case 'c':
            getTime = getCTime;
            break;
        case 'f':
            unsorted = true;
            selectEntry = selectAll;
            break;
        case 'g':
            output = outputLong;
            noOwner = true;
            break;
        case 'i':
            printInode = true;
            break;
        case 'l':
            output = outputLong;
            break;
        case 'n':
            output = outputLong;
            numericUidGid = true;
            break;
        case 'o':
            output = outputLong;
            noGroup = true;
            break;
        case 't':
            sort = sortByTime;
            break;
        case 'u':
            getTime = getATime;
            break;
        case '1':
            output = outputOneline;
            break;
        case '?':
            return 1;
        }
    }

    struct DirListing listing;
    listing.entries = NULL;
    listing.numEntries = 0;
    listing.allocated = 0;

    if (optind >= argc) {
        if (getDirectoryEntries(&listing, ".")) {
            list(&listing);
        }
        freeEntries(&listing);
        free(listing.entries);
        return success ? 0 : 1;
    }

    for (int i = optind; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0 || !S_ISDIR(st.st_mode)) {
            addEntry(&listing, AT_FDCWD, argv[i]);
            argv[i] = NULL;
        }
    }

    list(&listing);
    const char* newline = listing.numEntries > 0 ? "\n" : "";
    freeEntries(&listing);

    for (int i = optind; i < argc; i++) {
        if (!argv[i]) continue;
        if (optind + 1 < argc) {
            printf("%s%s:\n", newline, argv[i]);
            newline = "\n";
        }

        if (getDirectoryEntries(&listing, argv[i])) {
            list(&listing);
        }
        freeEntries(&listing);
    }

    free(listing.entries);
    return success ? 0 : 1;
}

static void addEntry(struct DirListing* listing, int dirFd, const char* name) {
    struct DirEntry* entry = malloc(sizeof(struct DirEntry));
    if (!entry) err(1, "malloc");

    entry->name = strdup(name);
    if (!entry->name) err(1, "strdup");

    if (fstatat(dirFd, entry->name, &entry->stat, AT_SYMLINK_NOFOLLOW) < 0) {
        success = false;
        warn("stat: '%s'", name);
        free(entry->name);
        free(entry);
        return;
    }

    if (listing->numEntries >= listing->allocated) {
        if (listing->allocated == 0) {
            listing->allocated = 4;
        }

        struct DirEntry** newBuffer = reallocarray(listing->entries,
                listing->allocated, 2 * sizeof(struct DirEntry*));
        if (!newBuffer) err(1, "reallocarray");
        listing->entries = newBuffer;
        listing->allocated *= 2;
    }
    listing->entries[listing->numEntries++] = entry;
}

static void freeEntries(struct DirListing* listing) {
    for (size_t i = 0; i < listing->numEntries; i++) {
        free(listing->entries[i]->name);
        free(listing->entries[i]);
    }
    listing->numEntries = 0;
}

static void getColor(mode_t mode, const char** pre, const char** post) {
    if (!colors) {
        *pre = "";
        *post = "";
        return;
    }
    *post = "\e[0m";

    if (S_ISDIR(mode)) {
        *pre = "\e[1;34m";
    } else if (S_ISBLK(mode) || S_ISCHR(mode)) {
        *pre = "\e[1;33m";
    } else if (S_ISFIFO(mode)) {
        *pre = "\e33m";
    } else if (S_ISLNK(mode)) {
        *pre = "\e[1;36m";
    } else if (S_ISSOCK(mode)) {
        *pre = "\e[1;35m";
    } else if (S_ISREG(mode) && mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        *pre = "\e[1;32m";
    } else {
        *pre = "";
        *post = "";
    }
}

static bool getDirectoryEntries(struct DirListing* listing, const char* path) {
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        success = false;
        warn("'%s'", path);
        return false;
    }
    DIR* dir = fdopendir(fd);
    if (!dir) {
        success = false;
        warn("fdopendir");
        close(fd);
        return false;
    }

    errno = 0;
    struct dirent* dirent = readdir(dir);
    while (dirent) {
        if (!selectEntry(dirent->d_name)) {
            errno = 0;
            dirent = readdir(dir);
            continue;
        }

        addEntry(listing, fd, dirent->d_name);
        errno = 0;
        dirent = readdir(dir);
    }

    if (errno != 0) err(1, "readdir");
    closedir(dir);
    return true;
}

static void list(struct DirListing* listing) {
    if (!unsorted) {
        qsort(listing->entries, listing->numEntries, sizeof(struct DirEntry*),
                (int (*)(const void*, const void*)) sort);
    }
    output(listing->entries, listing->numEntries);
}

static void printMode(mode_t mode) {
    char buffer[11];

    buffer[0] = S_ISBLK(mode) ? 'b' :
            S_ISCHR(mode) ? 'c' :
            S_ISDIR(mode) ? 'd' :
            S_ISLNK(mode) ? 'l' :
            S_ISFIFO(mode) ? 'p' :
            S_ISSOCK(mode) ? 's' :
            S_ISREG(mode) ? '-' : '?';

    buffer[1] = mode & S_IRUSR ? 'r' : '-';
    buffer[2] = mode & S_IWUSR ? 'w' : '-';
    buffer[3] = mode & S_IXUSR ? 'x' : '-';
    buffer[4] = mode & S_IRGRP ? 'r' : '-';
    buffer[5] = mode & S_IWGRP ? 'w' : '-';
    buffer[6] = mode & S_IXGRP ? 'x' : '-';
    buffer[7] = mode & S_IROTH ? 'r' : '-';
    buffer[8] = mode & S_IWOTH ? 'w' : '-';
    buffer[9] = mode & S_IXOTH ? 'x' : '-';
    buffer[10] = '\0';

    fputs(buffer, stdout);
}

static struct timespec getATime(struct DirEntry* entry) {
    return entry->stat.st_atim;
}

static struct timespec getCTime(struct DirEntry* entry) {
    return entry->stat.st_ctim;
}

static struct timespec getMTime(struct DirEntry* entry) {
    return entry->stat.st_mtim;
}

static void outputColumns(struct DirEntry** entries, size_t numEntries) {
    size_t lineWidth;

    struct winsize ws;
    if (tcgetwinsize(1, &ws) < 0) {
        lineWidth = 80;
    } else {
        lineWidth = ws.ws_col;
    }

    int inodeFieldLength = 0;
    size_t nameFieldLength = 0;

    for (size_t i = 0; i < numEntries; i++) {
        if (printInode) {
            int length = snprintf(NULL, 0, "%ju",
                    (uintmax_t) entries[i]->stat.st_ino);
            if (length > inodeFieldLength) {
                inodeFieldLength = length;
            }
        }

        size_t length = strlen(entries[i]->name);
        if (length > nameFieldLength) {
            nameFieldLength = length;
        }
    }

    size_t maxLength = nameFieldLength;
    if (printInode) {
        maxLength += 1 + inodeFieldLength;
    }

    size_t columns = (lineWidth + 1) / (maxLength + 1);
    if (columns == 0) {
        columns = 1;
    }

    size_t lines = (numEntries + columns - 1) / columns;

    for (size_t line = 0; line < lines; line++) {
        for (size_t column = 0; column < columns; column++) {
            size_t index = column * lines + line;
            if (index >= (size_t) numEntries) {
                putchar('\n');
                break;
            }

            struct DirEntry* entry = entries[index];
            if (printInode) {
                printf("%*ju ", inodeFieldLength,
                        (uintmax_t) entry->stat.st_ino);
            }

            const char* pre;
            const char* post;
            getColor(entry->stat.st_mode, &pre, &post);

            printf("%s%-*s%s%c", pre, (int) nameFieldLength, entry->name, post,
                    column == columns - 1 ? '\n' : ' ');
        }
    }
}

static void outputLong(struct DirEntry** entries, size_t numEntries) {
    int inodeFieldLength = 0;
    int nlinkFieldLength = 0;
    int ownerFieldLength = 0;
    int groupFieldLength = 0;
    int sizeFieldLength = 0;
    int dateFieldLength = 0;

    char buffer[100];
    time_t now = time(NULL);

    for (size_t i = 0; i < numEntries; i++) {
        struct DirEntry* entry = entries[i];
        int length;
        if (printInode) {
            length = snprintf(NULL, 0, "%ju", (uintmax_t) entry->stat.st_ino);
            if (length > inodeFieldLength) {
                inodeFieldLength = length;
            }
        }
        length = snprintf(NULL, 0, "%ju", (uintmax_t) entry->stat.st_nlink);
        if (length > nlinkFieldLength) {
            nlinkFieldLength = length;
        }
        if (!noOwner) {
            length = snprintf(NULL, 0, "%ju", (uintmax_t) entry->stat.st_uid);
            if (length > ownerFieldLength) {
                ownerFieldLength = length;
            }
        }
        if (!noGroup) {
            length = snprintf(NULL, 0, "%ju", (uintmax_t) entry->stat.st_gid);
            if (length > groupFieldLength) {
               groupFieldLength = length;
            }
        }
        length = snprintf(NULL, 0, "%ju", (uintmax_t) entry->stat.st_size);
        if (length > sizeFieldLength) {
            sizeFieldLength = length;
        }

        time_t entryTime = getTime(entry).tv_sec;
        struct tm* tm = localtime(&entryTime);
        const char* format = "%b %e %H:%M";
        if (now - sixMonths >= entryTime || entryTime > now) {
            format = "%b %e  %Y";
        }

        length = strftime(buffer, sizeof(buffer), format, tm);
        if (length > dateFieldLength) {
            dateFieldLength = length;
        }
    }

    for (size_t i = 0; i < numEntries; i++) {
        struct DirEntry* entry = entries[i];
        if (printInode) {
            printf("%*ju ", inodeFieldLength, (uintmax_t) entry->stat.st_ino);
        }
        printMode(entry->stat.st_mode);
        printf(" %*ju ", nlinkFieldLength, (uintmax_t) entry->stat.st_nlink);

        if (!numericUidGid) {
            // TODO: print owner/group name instead of uid/gid.
        }
        if (!noOwner) {
            printf("%*ju ", ownerFieldLength, (uintmax_t) entry->stat.st_uid);
        }
        if (!noGroup) {
            printf("%*ju ", groupFieldLength, (uintmax_t) entry->stat.st_gid);
        }
        printf("%*ju ", sizeFieldLength, (uintmax_t) entry->stat.st_size);

        time_t entryTime = getTime(entry).tv_sec;
        struct tm* tm = localtime(&entryTime);
        const char* format = "%b %e %H:%M";
        if (now - sixMonths >= entryTime || entryTime > now) {
            format = "%b %e  %Y";
        }
        if (strftime(buffer, sizeof(buffer), format, tm) == 0) {
            strcpy(buffer, "?");
        }
        printf("%*s ", dateFieldLength, buffer);

        const char* pre;
        const char* post;
        getColor(entry->stat.st_mode, &pre, &post);

        // TODO: Print symlink targets.
        printf("%s%s%s\n", pre, entry->name, post);
    }
}

static void outputOneline(struct DirEntry** entries, size_t numEntries) {
    for (size_t i = 0; i < numEntries; i++) {
        if (printInode) {
            printf("%ju ", (uintmax_t) entries[i]->stat.st_ino);
        }

        const char* pre;
        const char* post;
        getColor(entries[i]->stat.st_mode, &pre, &post);
        printf("%s%s%s\n", pre, entries[i]->name, post);
    }
}

static bool selectAll(const char* name) {
    (void) name;
    return true;
}

static bool selectAlmostAll(const char* name) {
    return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static bool selectDefault(const char* name) {
    return name[0] != '.';
}

static int sortByName(struct DirEntry** entry1, struct DirEntry** entry2) {
    return strcoll((*entry1)->name, (*entry2)->name);
}

static int sortBySize(struct DirEntry** entry1, struct DirEntry** entry2) {
    off_t size1 = (*entry1)->stat.st_size;
    off_t size2 = (*entry2)->stat.st_size;

    if (size1 < size2) {
        return 1;
    } else if (size1 > size2) {
        return -1;
    } else {
        return sortByName(entry1, entry2);
    }
}

static int sortByTime(struct DirEntry** entry1, struct DirEntry** entry2) {
    struct timespec time1 = getTime(*entry1);
    struct timespec time2 = getTime(*entry2);

    if (time1.tv_sec < time2.tv_sec) {
        return 1;
    } else if (time1.tv_sec > time2.tv_sec) {
        return -1;
    } else if (time1.tv_nsec < time2.tv_nsec) {
        return 1;
    } else if (time1.tv_nsec > time2.tv_nsec) {
        return -1;
    } else {
        return sortByName(entry1, entry2);
    }
}
