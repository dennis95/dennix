# Dennix

Dennix is a unix-like hobbyist operating system for x86 and x86_64 written
from scratch. It includes its own monolithic kernel written in C++, a standard
C library, a shell and userspace utilities written in C.

## Building

To build Dennix you will first need to install a cross toolchain for Dennix.
The command `make install-toolchain` will download, build and install the
toolchain. The installation script can be configured using environment
variables. You can use the command `./build-aux/install-toolchain.sh --help`
to get information about these environment variables.

You will probably want to set `$PREFIX` to a path where you want the toolchain
to be installed. After the toolchain has been installed you need to add
`$PREFIX/bin` to your `$PATH`. Finally you can run `make` to build a bootable
cdrom image.

## License

Dennix is free software and is licensed under the terms of the ISC license. The
full license terms can be found in the `LICENSE` file. The math library (libm)
code is licensed under the MIT license and other permissive licenses compatible
to the ISC license. See the `libm/COPYRIGHT` file for details.

Please note that this license only applies to Dennix itself. Any third-party
ports that are included in release images are released under their own
licenses. The licenses for all ports are available in the `/share/licenses`
directory of the release image.
