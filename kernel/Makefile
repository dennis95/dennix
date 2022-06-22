# Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021 Dennis Wölfing
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
CXXFLAGS += -fno-rtti -fcheck-new -nostdinc++ -fstack-protector-strong
CXXFLAGS += -Wall -Wextra -Wno-missing-field-initializers
CPPFLAGS += -I include -DDENNIX_VERSION=\"$(VERSION)\"
CPPFLAGS += -D__is_dennix_kernel -D_DENNIX_SOURCE
LDFLAGS += --sysroot=$(SYSROOT) -T $(LDSCRIPT) -ffreestanding -nostdlib
LIBS += -lk -lgcc

OBJ = \
	acpi.o \
	addressspace.o \
	ata.o \
	bga.o \
	blockcache.o \
	circularbuffer.o \
	clock.o \
	conf.o \
	console.o \
	cxx.o \
	devices.o \
	directory.o \
	display.o \
	ext234fs.o \
	ext234vnode.o \
	file.o \
	filedescription.o \
	hpet.o \
	initrd.o \
	kernel.o \
	keyboard.o \
	kthread.o \
	libk.o \
	log.o \
	memorysegment.o \
	mouse.o \
	panic.o \
	partition.o \
	pci.o \
	physicalmemory.o \
	pipe.o \
	pit.o \
	process.o \
	ps2.o \
	ps2keyboard.o \
	ps2mouse.o \
	pseudoterminal.o \
	random.o \
	refcount.o \
	rtc.o \
	signal.o \
	streamsocket.o \
	symlink.o \
	syscall.o \
	terminal.o \
	thread.o \
	vnode.o \
	worker.o

all: $(BUILD)/kernel

include $(ARCH).mk

OBJ := $(addprefix $(BUILD)/, $(OBJ))
-include $(OBJ:.o=.d)

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

$(BUILD)/%.o: $(BUILD)/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CXX) $(ASFLAGS) $(CPPFLAGS) -MD -MP -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
