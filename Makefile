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

REPO_ROOT = .

export ARCH ?= i686

include $(REPO_ROOT)/build-aux/paths.mk
include $(REPO_ROOT)/build-aux/toolchain.mk

KERNEL = $(BUILD_DIR)/kernel/kernel
ISO = dennix.iso

all: kernel iso

kernel:
	$(MAKE) -C kernel

iso: $(ISO)

$(ISO): $(KERNEL)
	rm -rf $(BUILD_DIR)/isosrc
	cp -rf isosrc $(BUILD_DIR)
	cp -f $(KERNEL) $(BUILD_DIR)/isosrc
	$(MKRESCUE) -o $@ $(BUILD_DIR)/isosrc

$(KERNEL):
	$(MAKE) -C kernel

qemu: $(ISO)
	qemu-system-i386 -cdrom $^

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(ISO)

.PHONY: all kernel iso qemu clean
