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

LIBM_DIR = $(REPO_ROOT)/libm
LIBM_BUILD = $(BUILD_DIR)/libm

LIBM_GENERIC_SRC = $(wildcard $(LIBM_DIR)/src/*/*.c)
LIBM_ARCH_SRC = $(wildcard $(LIBM_DIR)/src/*/$(BASE_ARCH)/*.[csS])
LIBM_GENERIC_OBJ = $(LIBM_GENERIC_SRC:.c=.o)
LIBM_ARCH_OBJ = $(addsuffix .o,$(basename $(LIBM_ARCH_SRC)))
LIBM_UNUSED_OBJ = $(subst /$(BASE_ARCH)/,/,$(LIBM_ARCH_OBJ))
LIBM_OBJ = $(filter-out $(LIBM_UNUSED_OBJ),$(LIBM_GENERIC_OBJ) $(LIBM_ARCH_OBJ))
LIBM_OBJ := $(patsubst $(LIBM_DIR)/src/%,$(LIBM_BUILD)/%,$(LIBM_OBJ))

-include $(LIBM_OBJ:.o=.d)

LIBM_CFLAGS ?= -O2 -g
LIBM_CFLAGS += --sysroot=$(SYSROOT) -std=gnu11 -ffreestanding
LIBM_CFLAGS += -fexcess-precision=standard -frounding-math
LIBM_CFLAGS += -Wall -Wno-maybe-uninitialized -Wno-parentheses
LIBM_CFLAGS += -Wno-unknown-pragmas -Wno-unused-but-set-variable
LIBM_CPPFLAGS = -I $(LIBM_DIR)/arch/$(BASE_ARCH) -I $(LIBM_DIR)/arch/generic
LIBM_CPPFLAGS += -I $(LIBM_DIR)/src/include -I $(LIBM_DIR)/src/internal
LIBM_CPPFLAGS += -I $(LIBM_DIR)/include $(CPPFLAGS)

$(LIBM_BUILD)/%.o: $(LIBM_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(LIBM_CFLAGS) $(LIBM_CPPFLAGS) -MD -MP -c -o $@ $<

$(LIBM_BUILD)/%.o: $(LIBM_DIR)/src/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(LIBM_BUILD)/%.o: $(LIBM_DIR)/src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(LIBM_CPPFLAGS) -c -o $@ $<

install-libm-headers: install-libm-license
	@mkdir -p $(INCLUDE_DIR)/bits
	cp -rf --preserve=timestamp $(LIBM_DIR)/arch/$(BASE_ARCH)/bits/. \
		$(INCLUDE_DIR)/bits
	cp -rf --preserve=timestamp $(LIBM_DIR)/include/. $(INCLUDE_DIR)

install-libm-license:
	@mkdir -p $(LICENSES_DIR)/libm
	cp -f $(LIBM_DIR)/COPYRIGHT $(LICENSES_DIR)/libm

.PHONY: install-libm-headers install-libm-license
