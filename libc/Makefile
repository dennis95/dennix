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

BUILD = $(BUILD_DIR)/libc

ARFLAGS = -crs
CFLAGS ?= -O2 -g
CFLAGS += --sysroot=$(SYSROOT) -std=gnu11 -ffreestanding -Wall -Wextra
CPPFLAGS += -I include -D_ALL_SOURCE

LIBC_FLAGS += -D__is_dennix_libc
LIBK_FLAGS += -D__is_dennix_libk

COMMON_OBJ = \
	crt/init \
	crt/fini \
	ctype/ctype \
	errno/errno \
	getopt/getopt \
	getopt/getopt_long \
	inttypes/strtoimax \
	inttypes/strtoumax \
	libgen/basename \
	libgen/dirname \
	sched/sched_yield \
	signal/sigaddset \
	signal/sigdelset \
	signal/sigemptyset \
	signal/sigfillset \
	signal/sigismember \
	stdio/asprintf \
	stdio/snprintf \
	stdio/sprintf \
	stdio/sscanf \
	stdio/vasprintf \
	stdio/vcbprintf \
	stdio/vcbscanf \
	stdio/vsnprintf \
	stdio/vsprintf \
	stdio/vsscanf\
	stdlib/abs \
	stdlib/arc4random \
	stdlib/arc4random_buf \
	stdlib/arc4random_uniform \
	stdlib/atoi \
	stdlib/atol \
	stdlib/atoll \
	stdlib/bsearch \
	stdlib/calloc \
	stdlib/div \
	stdlib/free \
	stdlib/labs \
	stdlib/ldiv \
	stdlib/llabs \
	stdlib/lldiv \
	stdlib/malloc \
	stdlib/malloc-util \
	stdlib/mblen \
	stdlib/mbstowcs \
	stdlib/mbtowc \
	stdlib/qsort \
	stdlib/qsort_r \
	stdlib/rand \
	stdlib/realloc \
	stdlib/reallocarray \
	stdlib/strtol \
	stdlib/strtoll \
	stdlib/strtoul \
	stdlib/strtoull \
	stdlib/wcstombs \
	stdlib/wctomb \
	string/explicit_bzero \
	string/memchr \
	string/memcmp \
	string/memcpy \
	string/memmove \
	string/memset \
	string/stpcpy \
	string/stpncpy \
	string/strcat \
	string/strchr \
	string/strcmp \
	string/strcpy \
	string/strcspn \
	string/strdup \
	string/strerror \
	string/strlcpy \
	string/strlen \
	string/strncat \
	string/strncmp \
	string/strncpy \
	string/strndup \
	string/strnlen \
	string/strpbrk \
	string/strrchr \
	string/strsignal \
	string/strspn \
	string/strstr \
	string/strtok \
	time/gmtime \
	time/gmtime_r \
	time/timegm \
	wchar/mbrtowc \
	wchar/mbsinit \
	wchar/mbsrtowcs \
	wchar/wcrtomb \
	wchar/wcschr \
	wchar/wcscmp \
	wchar/wcscpy \
	wchar/wcslen \
	wchar/wcsrtombs \
	wchar/wcwidth

LIBC_OBJ = \
	$(COMMON_OBJ) \
	assert/assert \
	devctl/posix_devctl \
	dirent/alphasort \
	dirent/closedir \
	dirent/dirfd \
	dirent/fdopendir \
	dirent/opendir \
	dirent/posix_getdents \
	dirent/readdir \
	dirent/rewinddir \
	dirent/scandir \
	err/err \
	err/errc \
	err/errx \
	err/verr \
	err/verrc \
	err/verrx \
	err/vwarn \
	err/vwarnc \
	err/vwarnx \
	err/warn \
	err/warnc \
	err/warnx \
	errno/initProgname \
	fcntl/fcntl \
	fcntl/open \
	fcntl/openat \
	grp/getgrgid \
	langinfo/nl_langinfo \
	locale/localeconv \
	locale/setlocale \
	poll/poll \
	poll/ppoll \
	pwd/getpwnam \
	search/tdelete \
	search/tfind \
	search/tsearch \
	search/twalk \
	signal/kill \
	signal/raise \
	signal/sigaction \
	signal/signal \
	signal/sigprocmask \
	signal/sigtimedwait \
	signal/sigwait \
	signal/sigwaitinfo \
	stdio/__file_read \
	stdio/__file_seek \
	stdio/__file_write \
	stdio/__fmodeflags \
	stdio/clearerr_unlocked \
	stdio/clearerr \
	stdio/dprintf \
	stdio/fclose \
	stdio/fdopen \
	stdio/feof_unlocked \
	stdio/feof \
	stdio/ferror_unlocked \
	stdio/ferror \
	stdio/fflush_unlocked \
	stdio/fflush \
	stdio/fgetc_unlocked \
	stdio/fgetc \
	stdio/fgetpos \
	stdio/fgets_unlocked \
	stdio/fgets \
	stdio/fileno \
	stdio/flockfile \
	stdio/fmemopen \
	stdio/fopen \
	stdio/fprintf \
	stdio/fputc_unlocked \
	stdio/fputc \
	stdio/fputs_unlocked \
	stdio/fputs \
	stdio/fread_unlocked \
	stdio/fread \
	stdio/freopen \
	stdio/fscanf \
	stdio/fseek \
	stdio/fseeko_unlocked \
	stdio/fseeko \
	stdio/fsetpos \
	stdio/ftell \
	stdio/ftello_unlocked \
	stdio/ftello \
	stdio/funlockfile \
	stdio/fwrite_unlocked \
	stdio/fwrite \
	stdio/getc_unlocked \
	stdio/getc \
	stdio/getchar_unlocked \
	stdio/getchar \
	stdio/getdelim \
	stdio/getline \
	stdio/perror \
	stdio/printf \
	stdio/putc_unlocked \
	stdio/putc \
	stdio/putchar_unlocked \
	stdio/putchar \
	stdio/puts \
	stdio/remove \
	stdio/rename \
	stdio/renameat \
	stdio/rewind \
	stdio/scanf \
	stdio/setbuf \
	stdio/setvbuf \
	stdio/stderr \
	stdio/stdin \
	stdio/stdout \
	stdio/tmpfile \
	stdio/ungetc_unlocked \
	stdio/ungetc \
	stdio/vdprintf \
	stdio/vfprintf_unlocked \
	stdio/vfprintf \
	stdio/vfscanf \
	stdio/vprintf \
	stdio/vscanf \
	stdlib/abort \
	stdlib/atexit \
	stdlib/atof \
	stdlib/canonicalize_file_name \
	stdlib/_Exit \
	stdlib/exit \
	stdlib/getenv \
	stdlib/mkdtemp \
	stdlib/mkostemp \
	stdlib/mkostemps \
	stdlib/mkstemp \
	stdlib/mkstemps \
	stdlib/realpath \
	stdlib/setenv \
	stdlib/strtod \
	stdlib/strtof \
	stdlib/strtold \
	stdlib/system \
	stdlib/unsetenv \
	string/strcoll \
	string/strxfrm \
	strings/strcasecmp \
	strings/strncasecmp \
	sys/fs/mount \
	sys/fs/unmount \
	sys/ioctl/ioctl \
	sys/mman/mmap \
	sys/mman/munmap \
	sys/resource/getrlimit \
	sys/resource/getrusage \
	sys/resource/getrusagens \
	sys/socket/accept \
	sys/socket/accept4 \
	sys/socket/bind \
	sys/socket/connect \
	sys/socket/listen \
	sys/socket/socket \
	sys/stat/chmod \
	sys/stat/fchmod \
	sys/stat/fchmodat \
	sys/stat/fstat \
	sys/stat/fstatat \
	sys/stat/futimens \
	sys/stat/lstat \
	sys/stat/mkdir \
	sys/stat/mkdirat \
	sys/stat/stat \
	sys/stat/umask \
	sys/stat/utimensat \
	sys/time/utimes \
	sys/utsname/uname \
	sys/wait/wait \
	sys/wait/waitpid \
	termios/tcflush \
	termios/tcgetattr \
	termios/tcgetwinsize \
	termios/tcsetattr \
	time/asctime \
	time/clock_gettime \
	time/clock_nanosleep \
	time/clock \
	time/ctime \
	time/difftime \
	time/localtime \
	time/mktime \
	time/nanosleep \
	time/strftime \
	time/time \
	time/tzset \
	unistd/access \
	unistd/alarm \
	unistd/chdir \
	unistd/chown \
	unistd/close \
	unistd/confstr \
	unistd/dup \
	unistd/dup2 \
	unistd/dup3 \
	unistd/environ \
	unistd/execl \
	unistd/execv \
	unistd/execve \
	unistd/execvp \
	unistd/_exit \
	unistd/fchdir \
	unistd/fchdirat \
	unistd/fchownat \
	unistd/fork \
	unistd/fpathconf \
	unistd/ftruncate \
	unistd/getcwd \
	unistd/getentropy \
	unistd/geteuid \
	unistd/getgid \
	unistd/gethostname \
	unistd/getlogin \
	unistd/getpid \
	unistd/getpgid \
	unistd/getuid \
	unistd/isatty \
	unistd/lchown \
	unistd/link \
	unistd/linkat \
	unistd/lseek \
	unistd/meminfo \
	unistd/pathconf \
	unistd/pipe \
	unistd/pipe2 \
	unistd/read \
	unistd/readlink \
	unistd/readlinkat \
	unistd/regfork \
	unistd/rmdir \
	unistd/setpgid \
	unistd/sleep \
	unistd/symlink \
	unistd/symlinkat \
	unistd/tcgetpgrp \
	unistd/tcsetpgrp \
	unistd/unlink \
	unistd/unlinkat \
	unistd/usleep \
	unistd/write

LIBK_OBJ = $(COMMON_OBJ)

EMPTY_LIBS = libm.a
LIBS = $(addprefix $(BUILD)/, libc.a libk.a $(EMPTY_LIBS))

all:

include $(REPO_ROOT)/libm/libm.mk
include $(ARCH).mk

LIBC_OBJ := $(addprefix $(BUILD)/, $(addsuffix .o, $(LIBC_OBJ)))
LIBK_OBJ := $(addprefix $(BUILD)/, $(addsuffix .libk.o, $(LIBK_OBJ)))

all: $(LIBS) $(CRT_OBJ)

-include $(LIBC_OBJ:.o=.d)
-include $(LIBK_OBJ:.o=.d)
LIBC_OBJ += $(LIBM_OBJ)

install: install-headers install-libs

install-headers: install-libm-headers
	cp -rf --preserve=timestamp include/. $(INCLUDE_DIR)
	touch $(SYSROOT)

install-libs: $(LIBS) $(CRT_OBJ)
	@mkdir -p $(LIB_DIR)
	cp -f --preserve=timestamp $^ $(LIB_DIR)
	touch $(SYSROOT)

$(BUILD)/libc.a: $(LIBC_OBJ)
$(BUILD)/libk.a: $(LIBK_OBJ)

$(BUILD)/%.a:
	$(AR) $(ARFLAGS) $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBC_FLAGS) -MD -MP -c -o $@ $<

$(BUILD)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) $(CPPFLAGS) $(LIBC_FLAGS) -c -o $@ $<

$(BUILD)/%.libk.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBK_FLAGS) -MD -MP -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all install install-headers install-libs clean
