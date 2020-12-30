# Copyright (c) 2020 Dennis Wölfing
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

BUILD = $(BUILD_DIR)/gui

CFLAGS ?= -O2 -g
CFLAGS += --sysroot=$(SYSROOT) -Wall -Wextra
CPPFLAGS += -D_DENNIX_SOURCE

OBJ = \
	connection.o \
	display.o \
	gui.o \
	keyboard.o \
	mouse.o \
	server.o \
	window.o

CURSORS = \
	arrow.rgba \
	resize_diagonal1.rgba \
	resize_diagonal2.rgba \
	resize_horizontal.rgba \
	resize_vertical.rgba

all: $(BUILD)/gui $(BUILD)/gui-test

OBJ := $(addprefix $(BUILD)/, $(OBJ))
-include $(OBJ:.o=.d)
-include $(BUILD)/gui-test.d

install: $(BUILD)/gui $(BUILD)/gui-test
	@mkdir -p $(BIN_DIR)
	cp -f $(BUILD)/gui $(BUILD)/gui-test $(BIN_DIR)
	@mkdir -p $(SYSROOT)/share/cursors
	cp -f $(CURSORS) $(SYSROOT)/share/cursors
	touch $(SYSROOT)

$(BUILD)/gui: $(OBJ)
	$(CC) -o $@ $^

$(BUILD)/gui-test: $(BUILD)/gui-test.o
	$(CC) -o $@ $^

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
