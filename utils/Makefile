# Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021, 2022 Dennis Wölfing
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

REPO_ROOT = ..

include $(REPO_ROOT)/build-aux/arch.mk
include $(REPO_ROOT)/build-aux/paths.mk
include $(REPO_ROOT)/build-aux/toolchain.mk
include $(REPO_ROOT)/build-aux/version.mk

BUILD = $(BUILD_DIR)/utils

CFLAGS ?= -O2 -g
CFLAGS += --sysroot=$(SYSROOT) -std=c11 -fstack-protector-strong -Wall -Wextra
CPPFLAGS += -D_DENNIX_SOURCE -DDENNIX_VERSION=\"$(VERSION)\"

BIN_PROGRAMS = \
	basename \
	cat \
	chmod \
	chvideomode \
	clear \
	cp \
	date \
	dirname \
	echo \
	editor \
	expr \
	false \
	head \
	kill \
	ln \
	ls \
	meminfo \
	mkdir \
	mv \
	printf \
	pwd \
	rm \
	rmdir \
	seq \
	sleep \
	snake \
	sync \
	test \
	tail \
	time \
	touch \
	true \
	uname

SBIN_PROGRAMS = \
	init \
	mount \
	umount

PROGRAMS = $(BIN_PROGRAMS) $(SBIN_PROGRAMS)

all: $(addprefix $(BUILD)/, $(PROGRAMS))

install: install-bin install-sbin

install-bin: $(addprefix $(BUILD)/, $(BIN_PROGRAMS))
	@mkdir -p $(BIN_DIR)
	cp -f $^ $(BIN_DIR)
	ln -f $(BIN_DIR)/test $(BIN_DIR)/[
	touch $(SYSROOT)

install-sbin: $(addprefix $(BUILD)/, $(SBIN_PROGRAMS))
	@mkdir -p $(SBIN_DIR)
	cp -f $^ $(SBIN_DIR)
	touch $(SYSROOT)

$(BUILD)/%: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

$(BUILD)/mv: cp.c rm.c
$(BUILD)/tail: head.c

clean:
	rm -rf $(BUILD)

.PHONY: all install install-bin install-sbin clean
