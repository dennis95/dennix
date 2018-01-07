# Copyright (c) 2016, 2017, 2018 Dennis Wölfing
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
include $(REPO_ROOT)/build-aux/version.mk

BUILD = $(BUILD_DIR)/kernel

CXXFLAGS ?= -O2 -g
CXXFLAGS += --sysroot=$(SYSROOT) -std=gnu++14 -ffreestanding -fno-exceptions
CXXFLAGS += -fno-rtti -Wall -Wextra
CPPFLAGS += -I include -DDENNIX_VERSION=\"$(VERSION)\"
CPPFLAGS += -D__is_dennix_kernel -D_DENNIX_SOURCE
LDFLAGS += --sysroot=$(SYSROOT) -T linker.ld -ffreestanding -nostdlib
LIBS += -lk -lgcc

OBJ = \
	addressspace.o \
	clock.o \
	conf.o \
	directory.o \
	file.o \
	filedescription.o \
	gdt.o \
	idt.o \
	initrd.o \
	interrupts.o \
	kernel.o \
	keyboard.o \
	kthread.o \
	libk.o \
	log.o \
	memorysegment.o \
	physicalmemory.o \
	pit.o \
	process.o \
	ps2.o \
	ps2keyboard.o \
	refcount.o \
	rtc.o \
	signal.o \
	symlink.o \
	syscall.o \
	terminal.o \
	timer.o \
	vgaterminal.o \
	vnode.o

include $(ARCH).mk

all: $(BUILD)/kernel

OBJ := $(addprefix $(BUILD)/, $(OBJ))
-include $(OBJ:.o=.d)

OBJ := $(START_OBJ) $(OBJ) $(END_OBJ)

install-headers:
	@mkdir -p $(INCLUDE_DIR)
	cp -rf --preserve=timestamp include/. $(INCLUDE_DIR)
	touch $(SYSROOT)

$(BUILD)/kernel: $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CXX) $(ASFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
