/* Copyright (c) 2021 Dennis Wölfing
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

/* libc/src/glob/glob.c
 * Pathname pattern matching.
 */

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool containsSpecial(const char* pattern, size_t patternLength,
        bool noEscape) {
    bool escaped = false;
    for (size_t i = 0; i < patternLength; i++) {
        if (pattern[i] == '\\' && !noEscape) {
            escaped = !escaped;
        } else if (!escaped && (pattern[i] == '*' || pattern[i] == '?' ||
                pattern[i] == '[')) {
            return true;
        } else {
            escaped = false;
        }
    }
    return false;
}

static bool addResult(char* path, glob_t* data, size_t* stringsAllocated) {
    if (data->gl_offs + data->gl_pathc + 1 >= *stringsAllocated) {
        char** newStrings = reallocarray(data->gl_pathv,
                *stringsAllocated, 2 * sizeof(char*));
        if (!newStrings) return false;
        data->gl_pathv = newStrings;
        *stringsAllocated *= 2;
    }

    data->gl_pathv[data->gl_offs + data->gl_pathc] = path;
    data->gl_pathc++;
    return true;
}

static int globComponent(const char* prefix, const char* pattern, int flags,
        int (*errfunc)(const char*, int), glob_t* data,
        size_t* stringsAllocated) {
    size_t componentLength = strcspn(pattern, "/");
    const char* nextComponent = pattern + componentLength;
    bool trailingSlash = *nextComponent == '/';
    while (*nextComponent == '/') nextComponent++;
    bool endOfPattern = *nextComponent == '\0';

    bool needsMatching = containsSpecial(pattern, componentLength,
            flags & GLOB_NOESCAPE);
    if (!needsMatching) {
        char* path = malloc(strlen(prefix) + componentLength + 2);
        if (!path) return GLOB_NOSPACE;
        strcpy(path, prefix);

        size_t j = strlen(path);
        for (size_t i = 0; i < componentLength; i++) {
            if (pattern[i] == '\\') {
                if (pattern[i + 1] == '\\') {
                    path[j] = pattern[i + 1];
                    i++;
                    j++;
                }
            } else {
                path[j] = pattern[i];
                j++;
            }
        }
        if (trailingSlash) {
            path[j] = '/';
            j++;
        }
        path[j] = '\0';

        if (endOfPattern) {
            struct stat st;
            if (stat(path, &st) != 0 && lstat(path, &st) != 0) {
                free(path);
            } else {
                if (flags & GLOB_MARK && !trailingSlash) {
                    if (S_ISDIR(st.st_mode)) {
                        strcat(path, "/");
                    }
                }
                if (!addResult(path, data, stringsAllocated)) {
                    free(path);
                    return GLOB_NOSPACE;
                }
            }
        } else {
            int result = globComponent(path, nextComponent, flags, errfunc,
                    data, stringsAllocated);
            if (result != 0) {
                free(path);
                return result;
            }
            free(path);
        }
    } else {
        char* component = strndup(pattern, componentLength);
        if (!component) return GLOB_NOSPACE;

        DIR* dir = opendir(*prefix ? prefix : ".");
        if (!dir) {
            if (errfunc(*prefix ? prefix : ".", errno) || (flags & GLOB_ERR)) {
                free(component);
                return GLOB_ABORTED;
            }
            free(component);
            return 0;
        }

        errno = 0;
        struct dirent* entry = readdir(dir);
        while (entry) {
            if (fnmatch(component, entry->d_name, FNM_PERIOD |
                    (flags & GLOB_NOESCAPE ? FNM_NOESCAPE : 0)) == 0) {
                char* path = malloc(strlen(prefix) + strlen(entry->d_name) + 2);
                if (!path) {
                    closedir(dir);
                    free(component);
                    return GLOB_NOSPACE;
                }
                stpcpy(stpcpy(stpcpy(path, prefix), entry->d_name),
                        trailingSlash ? "/" : "");
                if (endOfPattern) {
                    if (flags & GLOB_MARK && !trailingSlash) {
                        if (entry->d_type == DT_DIR) {
                            strcat(path, "/");
                        } else if (entry->d_type == DT_UNKNOWN ||
                                entry->d_type == DT_LNK) {
                            struct stat st;
                            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                                strcat(path, "/");
                            }
                        }
                    }

                    if (!addResult(path, data, stringsAllocated)) {
                        free(path);
                        closedir(dir);
                        free(component);
                        return GLOB_NOSPACE;
                    }
                } else {
                    int result = globComponent(path, nextComponent, flags,
                            errfunc, data, stringsAllocated);
                    if (result != 0) {
                        free(path);
                        closedir(dir);
                        free(component);
                        return result;
                    }
                    free(path);
                }
            }

            errno = 0;
            entry = readdir(dir);
        }

        if (errno) {
            if (errfunc(*prefix ? prefix : ".", errno) || (flags & GLOB_ERR)) {
                closedir(dir);
                free(component);
                return GLOB_ABORTED;
            }
        }

        closedir(dir);
        free(component);
    }

    return 0;
}

static int onError(const char* path, int error) {
    (void) path;
    // Only ignore errors that are related to filesystem contents.
    return error != EACCES && error != ELOOP && error != ENAMETOOLONG &&
            error != ENOENT && error != ENOTDIR;
}

static int compare(const void* a, const void* b) {
    return strcoll(*(const char**) a, *(const char**) b);
}

int glob(const char* restrict pattern, int flags,
        int (*errfunc)(const char*, int), glob_t* restrict results) {
    if (!(flags & GLOB_DOOFFS)) {
        results->gl_offs = 0;
    }

    size_t stringsAllocated;
    if (flags & GLOB_APPEND) {
        stringsAllocated = results->gl_offs + results->gl_pathc + 1;
        if (stringsAllocated < 32) stringsAllocated = 32;
    } else {
        results->gl_pathc = 0;
        stringsAllocated = results->gl_offs + 32;
        results->gl_pathv = malloc(stringsAllocated * sizeof(char*));
        if (!results->gl_pathv) return GLOB_NOSPACE;
    }
    size_t oldCount = results->gl_pathc;

    for (size_t i = 0; i < results->gl_offs; i++) {
        results->gl_pathv[i] = NULL;
    }

    const char* originalPattern = pattern;
    const char* prefix = "";
    while (*pattern == '/') {
        prefix = "/";
        pattern++;
    }

    if (!errfunc) errfunc = onError;

    if (!*pattern) {
        if (*originalPattern) {
            char* str = strdup("/");
            if (!str) {
                results->gl_pathv[results->gl_offs + results->gl_pathc] = NULL;
                return GLOB_NOSPACE;
            }
            addResult(str, results, &stringsAllocated);
        }
    } else {
        int result = globComponent(prefix, pattern, flags, errfunc, results,
                &stringsAllocated);
        if (result) {
            results->gl_pathv[results->gl_offs + results->gl_pathc] = NULL;
            return result;
        }
    }

    if (flags & GLOB_NOCHECK && results->gl_pathc - oldCount == 0) {
        char* str = strdup(originalPattern);
        if (!str) {
            results->gl_pathv[results->gl_offs + results->gl_pathc] = NULL;
            return GLOB_NOSPACE;
        }
        addResult(str, results, &stringsAllocated);
    }

    if (!(flags & GLOB_NOSORT)) {
        qsort(results->gl_pathv + results->gl_offs + oldCount,
                results->gl_pathc - oldCount, sizeof(char*), compare);
    }

    results->gl_pathv[results->gl_offs + results->gl_pathc] = NULL;
    return results->gl_pathc ? 0 : GLOB_NOMATCH;
}
