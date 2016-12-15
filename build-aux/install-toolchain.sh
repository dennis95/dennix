#! /bin/sh

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

help='This script installs the dennix toolchain.
The following environment variables can be set to change the behavior:
PREFIX: The directory the toolchain will be installed.
        (default: $HOME/dennix-toolchain)
SRCDIR: The directory where the sources will be put into. (default: $HOME/src)
BUILDDIR: The directory where the build files will be put into.
          (default: $SRCDIR)
ARCH: The architecture the toolchain is built for. (default: i686)
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
[ -z "$ARCH" ] && ARCH=i686
[ -z "$TARGET" ] && TARGET=$ARCH-dennix

[ -z "$SYSROOT" ] && echo "Error: \$SYSROOT not set" && exit 1

if [ "$CONTINUOUS_INTEGRATION" = true ]
then
    # When built by Travis CI check whether the toolchain needs to be rebuilt.
    current_binutils=$(cat "$PREFIX/binutils-commit" || echo Not installed)
    current_gcc=$(cat "$PREFIX/gcc-commit" || echo Not installed)
    latest_binutils=$(git ls-remote $binutils_repo HEAD | cut -c 1-40)
    latest_gcc=$(git ls-remote $gcc_repo HEAD | cut -c 1-40)
    [ "$current_binutils" = "$latest_binutils" ] && \
        [ "$current_gcc" = "$latest_gcc" ] && \
        echo Cached Toolchain is already up to date. && exit
    rm -rf "$PREFIX"
fi

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

if [ "$CONTINUOUS_INTEGRATION" = true ]
then
    cd "$SRCDIR/dennix-binutils"
    git rev-parse HEAD > "$PREFIX/binutils-commit"
    cd "$SRCDIR/dennix-gcc"
    git rev-parse HEAD > "$PREFIX/gcc-commit"
fi

echo Installation completed.
