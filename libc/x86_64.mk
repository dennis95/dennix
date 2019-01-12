# Copyright (c) 2019 Dennis Wölfing
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

LIBK_FLAGS += -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2

CRT_OBJ = \
	$(BUILD)/arch/x86_64/crt0.o \
	$(BUILD)/arch/x86_64/crti.o \
	$(BUILD)/arch/x86_64/crtn.o

LIBC_OBJ += \
	arch/x86_64/rfork \
	arch/x86_64/setjmp \
	arch/x86_64/syscall
