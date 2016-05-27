# Copyright (c) 2016, Dennis Wölfing
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

ARCH ?= i686

REPO_ROOT = ..

include $(REPO_ROOT)/build-aux/paths.mk
include $(REPO_ROOT)/build-aux/toolchain.mk

BUILD = $(BUILD_DIR)/libc

ARFLAGS := -crs
CFLAGS ?= -O2 -g
CFLAGS += --sysroot=$(SYSROOT) -std=gnu11 -ffreestanding -Wall -Wextra
CXXFLAGS ?= -O2 -g
CXXFLAGS += --sysroot=$(SYSROOT) -std=gnu++14 -ffreestanding -Wall -Wextra
CPPFLAGS += -I include -I $(SYSROOT)/usr/include -D_ALL_SOURCE

LIBC_FLAGS += -D__is_dennix_libc
LIBK_FLAGS += -D__is_dennix_libk

COMMON_OBJ := \
	cxx/new \
	errno/errno \
	string/memcmp \
	string/memcpy \
	string/memmove \
	string/memset \
	string/strlen

LIBC_OBJ := \
	$(COMMON_OBJ) \
	stdio/flockfile \
	stdio/fputc_unlocked \
	stdio/fputc \
	stdio/fputs_unlocked \
	stdio/fputs \
	stdio/funlockfile \
	stdio/putc_unlocked \
	stdio/putc \
	stdio/putchar_unlocked \
	stdio/putchar \
	stdio/puts \
	stdio/stdout \
	stdlib/_Exit \
	stdlib/exit \
	unistd/_exit \
	unistd/write

LIBK_OBJ := \
	$(COMMON_OBJ) \
	stdlib/free \
	stdlib/malloc \
	stdlib/malloc-util

include $(ARCH).mk

LIBC_OBJ := $(addprefix $(BUILD)/, $(addsuffix .o, $(LIBC_OBJ)))
LIBK_OBJ := $(addprefix $(BUILD)/, $(addsuffix .libk.o, $(LIBK_OBJ)))

all: $(BUILD)/libc.a $(BUILD)/libk.a $(CRT_OBJ)

install: install-headers install-libs

install-headers: $(SYSROOT)/usr/include

install-libs: $(BUILD)/libc.a $(BUILD)/libk.a $(CRT_OBJ)
	@mkdir -p $(SYSROOT)/usr/lib
	cp -f $^ $(SYSROOT)/usr/lib

$(SYSROOT)/usr/include: include $(REPO_ROOT)/kernel/include
	@mkdir -p $@
	@touch $@
	cp -rf $^ $(SYSROOT)/usr

$(BUILD)/libc.a: $(LIBC_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD)/libk.a: $(LIBK_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBC_FLAGS) -c -o $@ $^

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LIBC_FLAGS) -c -o $@ $^

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(CPPFLAGS) $(LIBC_FLAGS) -c -o $@ $^

$(BUILD)/%.libk.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBK_FLAGS) -c -o $@ $^

$(BUILD)/%.libk.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LIBK_FLAGS) -c -o $@ $^

clean:
	rm -rf $(BUILD)

.PHONY: all install install-headers install-libs clean
