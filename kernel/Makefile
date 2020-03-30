# Copyright (c) 2016, 2017, 2018, 2019, 2020 Dennis Wölfing
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

BUILD = $(BUILD_DIR)/kernel
LDSCRIPT = $(BUILD)/linker.ld

CXXFLAGS ?= -O2 -g
CXXFLAGS += --sysroot=$(SYSROOT) -std=gnu++14 -ffreestanding -fno-exceptions
CXXFLAGS += -fno-rtti -fcheck-new -nostdinc++
CXXFLAGS += -Wall -Wextra -Wno-missing-field-initializers
CPPFLAGS += -I include -DDENNIX_VERSION=\"$(VERSION)\"
CPPFLAGS += -D__is_dennix_kernel -D_DENNIX_SOURCE
LDFLAGS += --sysroot=$(SYSROOT) -T $(LDSCRIPT) -ffreestanding -nostdlib
LIBS += -lk -lgcc

OBJ = \
	addressspace.o \
	clock.o \
	conf.o \
	cxx.o \
	devices.o \
	directory.o \
	file.o \
	filedescription.o \
	initrd.o \
	kernel.o \
	keyboard.o \
	kthread.o \
	libk.o \
	lfbdisplay.o \
	log.o \
	memorysegment.o \
	panic.o \
	pci.o \
	physicalmemory.o \
	pipe.o \
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
	terminaldisplay.o \
	thread.o \
	vgatextdisplay.o \
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

$(BUILD)/kernel: $(OBJ) $(LDSCRIPT)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

$(LDSCRIPT): linker.ld.in
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -P -xc -o $@ $<

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CXX) $(ASFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
