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

#include <fnmatch.h>
#include <stdio.h>

struct test {
    const char* pattern;
    const char* string;
    int flags;
    int result;
} tests[] = {
    { "a*c" , "abc", 0, 0 },
    { "*x*y*z", "abcxydez", 0, 0 },
    { "*x*y*z", "abcxydezf", 0, FNM_NOMATCH },
    { "??", "x", 0, FNM_NOMATCH },
    { "a", "A", 0, FNM_NOMATCH },
    { "a?c", "abc", 0, 0 },
    { "a?c", "abdc", 0, FNM_NOMATCH },
    { "a?", "a", 0, FNM_NOMATCH },
    { "a[bc]d", "abd", 0, 0 },
    { "a[bc]d", "acd", 0, 0 },
    { "[a-c]", "b", 0, 0 },
    { "[!a-c]", "b", 0, FNM_NOMATCH },
    { "[!a-c]", "d", 0, 0 },
    { "[?]", "?", 0, 0 },
    { "[?]", "x", 0, FNM_NOMATCH },
    { "[]-_]", "^", 0, 0 },
    { "[!]]", "x", 0, 0 },
    { "[!]]", "]", 0, FNM_NOMATCH },
    { "[/\\]", "/", 0, 0 },
    { "[/\\]", "\\", 0, 0 },
    { "[[:upper:]][[:punct:]]", "A.", 0, 0 },
    { "[[:upper:]][[:punct:]]", "a.", 0, FNM_NOMATCH },
    { "[![:upper:]]", "b", 0, 0 },
    { "[[:blank:]]", "\t", 0, 0 },
    { "[[:punct:]]", ".", FNM_PERIOD, FNM_NOMATCH },
    { "[\\[:alpha:]]", "b", 0, 0 },
    { "*a", ".a", 0, 0 },
    { "*a", ".a", FNM_PATHNAME, 0 },
    { "*a", ".a", FNM_PERIOD, FNM_NOMATCH },
    { "*a", ".a", FNM_PERIOD | FNM_PATHNAME, FNM_NOMATCH },
    { ".a", ".a", FNM_PERIOD, 0 },
    { ".a", ".a", FNM_PERIOD | FNM_PATHNAME, 0 },
    { "a*c", "a/c", 0, 0 },
    { "a*c", "a/c", FNM_PERIOD, 0 },
    { "a*c", "a/c", FNM_PATHNAME, FNM_NOMATCH },
    { "a[b/c]d", "a/d", 0, 0 },
    { "a[b/c]d", "a/d", FNM_PATHNAME, FNM_NOMATCH },
    { "a[b/c]d", "a[b/c]d", FNM_PATHNAME, 0 },
    { "a/*b", "a/.b", 0, 0 },
    { "a/*b", "a/.b", FNM_PERIOD, 0 },
    { "a/*b", "a/.b", FNM_PATHNAME, 0 },
    { "a/*b", "a/.b", FNM_PATHNAME | FNM_PERIOD, FNM_NOMATCH },
    { "a\\*c", "abc", 0, FNM_NOMATCH },
    { "a\\*c", "a*c", 0, 0 },
    { "a\\*c", "a*c", FNM_NOESCAPE, FNM_NOMATCH },
    { "a\\*c", "a\\bc", FNM_NOESCAPE, 0 },
    { "a\\bc", "abc", 0, 0},
    { "a\\bc", "abc", FNM_NOESCAPE, FNM_NOMATCH},
    { "a\\\\c", "a\\c", 0, 0 },
    { "[x", "[x", 0, 0 },
    { "\\/", "/", FNM_PATHNAME, 0 },
    { "[[.x.]]", "x", 0, 0 },
    { "[[.x.]]", "y", 0, FNM_NOMATCH },
    { "[![.x.]]", "a", 0, 0 },
    { "[[.x.]-[.z.]]", "y", 0, 0 },
    { "[[.x.]-[.z.]]", "w", 0, FNM_NOMATCH },
    { "[[=a=]]", "a", 0, 0 },
    { "[[=a=]]", "b", 0, FNM_NOMATCH },
    { "[[.[.]]", "[", 0, 0 },
    { "[[.].]]", "]", 0, 0 },
    { "[[.!.]]", "!", 0, 0 },
    { "[[.^.]]", "^", 0, 0 },
    { "[[.-.]]", "-", 0, 0 },
    { "[[.!.][.^.]]", "^", 0, 0 },
    { "[[.!.][.^.]]", "!^", 0, FNM_NOMATCH },
    { "[[.xyz.]]", "x", 0, FNM_NOMATCH },
    { "[[...]]", ".", 0, 0 },
    { "[[.\\.]]", "\\", 0, 0 },
    { "[[:]", "[:", 0, 0 },
    { "[[:]", ":", 0, FNM_NOMATCH },
    { "[[:]", "[[:]", 0, FNM_NOMATCH },
    { "[![:x]:]]", "x", 0, 0 },

#ifdef FNM_CASEFOLD
    { "a", "A", FNM_CASEFOLD, 0 },
    { "A", "a", FNM_CASEFOLD, 0 },
    { "aBcD", "AbcD", FNM_CASEFOLD, 0 },
    { "[a]", "A", FNM_CASEFOLD, 0 },
    { "[a-c]", "B", FNM_CASEFOLD, 0 },
    { "[!a-c]", "B", FNM_CASEFOLD, FNM_NOMATCH },
    { "[[:upper:]]", "b", FNM_CASEFOLD, 0 },
#endif

    { NULL, NULL, 0, 0 }
};

int main(void) {
    int status = 0;

    struct test* test = tests;
    while (test->pattern) {
        int result = fnmatch(test->pattern, test->string, test->flags);
        if (result != test->result) {
            printf("fnmatch(\"%s\", \"%s\", %d) = %d, expected %d\n",
                    test->pattern, test->string, test->flags, result,
                    test->result);
            status = 1;
        }

        test++;
    }

    return status;
}
