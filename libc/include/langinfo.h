/* Copyright (c) 2020 Dennis Wölfing
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

/* libc/include/langinfo.h
 * Language and locale information.
 */

#ifndef _LANGINFO_H
#define _LANGINFO_H

#include <sys/cdefs.h>
#define __need_locale_t
#include <bits/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int nl_item;

/* LC_CTYPE */
#define CODESET 0

/* LC_TIME */
#define D_T_FMT 100
#define D_FMT 101
#define T_FMT 102
#define T_FMT_AM_PM 103
#define AM_STR 104
#define PM_STR 105
#define DAY_1 106
#define DAY_2 107
#define DAY_3 108
#define DAY_4 109
#define DAY_5 110
#define DAY_6 111
#define DAY_7 112
#define ABDAY_1 113
#define ABDAY_2 114
#define ABDAY_3 115
#define ABDAY_4 116
#define ABDAY_5 117
#define ABDAY_6 118
#define ABDAY_7 119
#define MON_1 120
#define MON_2 121
#define MON_3 122
#define MON_4 123
#define MON_5 124
#define MON_6 125
#define MON_7 126
#define MON_8 127
#define MON_9 128
#define MON_10 129
#define MON_11 130
#define MON_12 131
#define ABMON_1 132
#define ABMON_2 133
#define ABMON_3 134
#define ABMON_4 135
#define ABMON_5 136
#define ABMON_6 137
#define ABMON_7 138
#define ABMON_8 139
#define ABMON_9 140
#define ABMON_10 141
#define ABMON_11 142
#define ABMON_12 143
#if __USE_DENNIX
#define ALTMON_1 144
#define ALTMON_2 145
#define ALTMON_3 146
#define ALTMON_4 147
#define ALTMON_5 148
#define ALTMON_6 149
#define ALTMON_7 150
#define ALTMON_8 151
#define ALTMON_9 152
#define ALTMON_10 153
#define ALTMON_11 154
#define ALTMON_12 155
#define ABALTMON_1 156
#define ABALTMON_2 157
#define ABALTMON_3 158
#define ABALTMON_4 159
#define ABALTMON_5 160
#define ABALTMON_6 161
#define ABALTMON_7 162
#define ABALTMON_8 163
#define ABALTMON_9 164
#define ABALTMON_10 165
#define ABALTMON_11 166
#define ABALTMON_12 167
#endif
#define ERA 168
#define ERA_D_FMT 169
#define ERA_D_T_FMT 170
#define ERA_T_FMT 171
#define ALT_DIGITS 172

/* LC_NUMERIC */
#define RADIXCHAR 200
#define THOUSEP 201

/* LC_MESSAGES */
#define YESEXPR 300
#define NOEXPR 301

/* LC_MONETARY */
#define CRNCYSTR 400

char* nl_langinfo(nl_item);

#ifdef __cplusplus
}
#endif

#endif
