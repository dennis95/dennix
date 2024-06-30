# Copyright (c) 2016, 2017, 2018, 2019, 2020, 2021, 2023, 2024 Dennis Wölfing
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

REPO_ROOT = .

include $(REPO_ROOT)/build-aux/arch.mk
include $(REPO_ROOT)/build-aux/paths.mk
include $(REPO_ROOT)/build-aux/toolchain.mk

JOBS = $(lastword $(filter -j%,$(MAKEFLAGS)))
COMPRESS = xz $(subst -j,-T,$(JOBS))

KERNEL = $(BUILD_DIR)/kernel/kernel
INITRD = $(BUILD_DIR)/initrd.tar.xz
ISO = dennix.iso
LICENSE = $(LICENSES_DIR)/dennix/LICENSE
DXPORT = ./ports/dxport --host=$(ARCH)-dennix --builddir=$(BUILD_DIR)/ports
DXPORT += --sysroot=$(SYSROOT)

all: libc kernel libdxui gui apps sh utils iso

apps: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C apps

gui: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C gui

kernel $(KERNEL): $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C kernel

libc: $(INCLUDE_DIR)
	$(MAKE) -C libc

libdxui: $(INCLUDE_DIR)
	$(MAKE) -C libdxui

install-all: install-headers install-libc install-libdxui install-gui
install-all: install-apps install-sh install-utils install-ports

install-apps:
	$(MAKE) -C apps install

install-gui:
	$(MAKE) -C gui install

install-headers $(INCLUDE_DIR):
	$(MAKE) -C kernel install-headers
	$(MAKE) -C libc install-headers
	$(MAKE) -C libdxui install-headers

install-libc: $(INCLUDE_DIR)
	$(MAKE) -C libc install-libs

install-libdxui: $(INCLUDE_DIR)
	$(MAKE) -C libdxui install-lib

$(LIB_DIR):
	$(MAKE) -C libc install-libs
	$(MAKE) -C libdxui install-lib

install-ports $(DXPORT_DIR): $(INCLUDE_DIR) $(LIB_DIR)
ifneq ($(wildcard ./ports/dxport),)
	-$(DXPORT) install -k $(JOBS) all
endif

install-sh: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C sh install

install-toolchain: install-headers
	SYSROOT=$(SYSROOT) $(REPO_ROOT)/build-aux/install-toolchain.sh

install-utils: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C utils install

iso: $(ISO)

$(ISO): $(KERNEL) $(INITRD)
	rm -rf $(BUILD_DIR)/isosrc
	mkdir -p $(BUILD_DIR)/isosrc/boot/grub
	cp -f build-aux/grub.cfg $(BUILD_DIR)/isosrc/boot/grub
	cp -f $(KERNEL) $(BUILD_DIR)/isosrc
	cp -f $(INITRD) $(BUILD_DIR)/isosrc
	$(MKRESCUE) -o $@ $(BUILD_DIR)/isosrc

$(INITRD): $(SYSROOT)
	cd $(SYSROOT) && tar -cf - --format=ustar * | $(COMPRESS) > ../$(INITRD)

qemu: $(ISO)
	qemu-system-$(BASE_ARCH) -cdrom $^ -m 1G -cpu host -enable-kvm

sh: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C sh

utils: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C utils

$(SYSROOT): $(INCLUDE_DIR) $(LIB_DIR) $(BIN_DIR) $(SYSROOT)/usr $(LICENSE)
$(SYSROOT): $(SYSROOT)/share/fonts/vgafont $(SYSROOT)/home/user $(DXPORT_DIR)

$(BIN_DIR):
	$(MAKE) -C gui install
	$(MAKE) -C apps install
	$(MAKE) -C sh install
	$(MAKE) -C utils install

$(LICENSE): LICENSE
	@mkdir -p $(LICENSES_DIR)/dennix
	cp -f LICENSE $@

$(SYSROOT)/usr:
	ln -s . $@

$(SYSROOT)/share/fonts/vgafont: kernel/vgafont
	@mkdir -p $(dir $@)
	cp -f kernel/vgafont $@

$(SYSROOT)/home/user:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(ISO)

distclean:
	rm -rf build sysroot
	rm -f *.iso

.NOTPARALLEL:
.PHONY: all apps gui kernel libc libdxui install-all install-apps install-gui
.PHONY: install-headers install-libc install-libdxui install-ports install-sh
.PHONY: install-toolchain install-utils iso qemu sh utils clean distclean
