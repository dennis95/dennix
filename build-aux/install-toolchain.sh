#! /bin/sh

# Copyright (c) 2016, 2019, 2020 Dennis Wölfing
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

help='This script installs the dennix toolchain.
The following environment variables can be set to change the behavior:
PREFIX: The directory the toolchain will be installed.
        (default: $HOME/dennix-toolchain)
SRCDIR: The directory where the sources will be put into. (default: $HOME/src)
BUILDDIR: The directory where the build files will be put into.
          (default: $SRCDIR)
ARCH: The architecture the toolchain is built for. (default: x86_64)
      This variable is ignored if $TARGET is set.
TARGET: The target of the toolchain. (default: $ARCH-dennix)
SYSROOT: The system root containing the system headers.
         This variable must be set. Using the make install-toolchain target of
         the root Makefile will set this variable automatically.'

set -e

binutils_repo=https://github.com/dennis95/dennix-binutils.git
gcc_repo=https://github.com/dennis95/dennix-gcc.git

([ "$1" = "--help" ] || [ "$1" = "-?" ]) && echo "$help" && exit

# Set some default values.
[ -z "${PREFIX+x}" ] && PREFIX="$HOME/dennix-toolchain"
[ -z "$SRCDIR" ] && SRCDIR="$HOME/src"
[ -z "$BUILDDIR" ] && BUILDDIR="$SRCDIR"
[ -z "$ARCH" ] && ARCH=x86_64
[ -z "$TARGET" ] && TARGET=$ARCH-dennix

[ -z "$SYSROOT" ] && echo "Error: \$SYSROOT not set" && exit 1

# Make $SYSROOT an absolute path
SYSROOT="$(cd "$SYSROOT" && pwd)"

export PATH="$PREFIX/bin:$PATH"

rm -rf "$SRCDIR/dennix-binutils" "$SRCDIR/dennix-gcc"
rm -rf "$BUILDDIR/build-binutils" "$BUILDDIR/build-gcc"

echo Downloading sources...
mkdir -p "$SRCDIR"
cd "$SRCDIR"
git clone $binutils_repo dennix-binutils
git clone $gcc_repo dennix-gcc

echo Building binutils...
mkdir -p "$BUILDDIR/build-binutils"
cd "$BUILDDIR/build-binutils"
"$SRCDIR/dennix-binutils/configure" --target=$TARGET --prefix="$PREFIX" \
  --with-sysroot="$SYSROOT" --disable-werror --disable-nls
make
make install

echo Building gcc...
mkdir -p "$BUILDDIR/build-gcc"
cd "$BUILDDIR/build-gcc"
"$SRCDIR/dennix-gcc/configure" --target=$TARGET --prefix="$PREFIX" \
  --with-sysroot="$SYSROOT" --enable-languages=c,c++ --disable-nls
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc

echo Installation completed.
