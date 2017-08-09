# Dennix

Dennix is a small unix-like hobbyist operating system written from scratch.
It includes its own monolithic kernel written in C++, a standard C library and
userspace applications written in C.

## Building

To build Dennix you will first need to install a cross toolchain for Dennix.
The command `make install-toolchain` will download, build and install the
toolchain. The installation script can be configured using environment
variables. You can use the command `./build-aux/install-toolchain.sh --help`
to get information about these environment variables.

You will probably want to set `$PREFIX` to a path where the toolchain should be
installed. After the toolchain has been installed you need to add `$PREFIX/bin`
to your `$PATH`. Finally you can run `make` to build a bootable cdrom image.

## License

Dennix is free software and is licensed under the terms of the ISC license. The
full license terms can be found in the `LICENSE` file.
