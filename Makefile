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

REPO_ROOT = .

include $(REPO_ROOT)/build-aux/arch.mk
include $(REPO_ROOT)/build-aux/paths.mk
include $(REPO_ROOT)/build-aux/toolchain.mk

KERNEL = $(BUILD_DIR)/kernel/kernel
INITRD = $(BUILD_DIR)/initrd.tar.xz
ISO = dennix.iso
LICENSE = $(LICENSES_DIR)/dennix/LICENSE
DXPORT = ./ports/dxport --host=$(ARCH)-dennix --builddir=$(BUILD_DIR)/ports
DXPORT += --sysroot=$(SYSROOT)

all: libc kernel sh utils iso

kernel $(KERNEL): $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C kernel

libc: $(INCLUDE_DIR)
	$(MAKE) -C libc

install-all: install-headers install-libc install-sh install-utils install-ports

install-headers $(INCLUDE_DIR):
	$(MAKE) -C kernel install-headers
	$(MAKE) -C libc install-headers

install-libc $(LIB_DIR): $(INCLUDE_DIR)
	$(MAKE) -C libc install-libs

install-ports $(DXPORT_DIR): $(INCLUDE_DIR) $(LIB_DIR)
ifneq ($(wildcard ./ports/dxport),)
	-$(DXPORT) install -k all
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
	cd $(SYSROOT) && tar cJf ../$(INITRD) --format=ustar *

qemu: $(ISO)
	qemu-system-$(BASE_ARCH) -cdrom $^ -m 512M

sh: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C sh

utils: $(INCLUDE_DIR) $(LIB_DIR)
	$(MAKE) -C utils

$(SYSROOT): $(INCLUDE_DIR) $(LIB_DIR) $(BIN_DIR) $(SYSROOT)/usr $(LICENSE)
$(SYSROOT): $(SYSROOT)/share/fonts/vgafont $(DXPORT_DIR)

$(BIN_DIR):
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

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(ISO)

distclean:
	rm -rf build sysroot
	rm -f *.iso

.PHONY: all kernel libc install-all install-headers install-libc install-ports
.PHONY: install-sh install-toolchain install-utils iso qemu sh utils clean
.PHONY: distclean
