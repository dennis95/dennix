# Dennix

Dennix is a small unix-like hobbyist operating system written from scratch.
It includes its own monolithic kernel written in C++, a standard C library and
a few utilities written in C.

## Building

To build Dennix you will need to install a cross toolchain for Dennix first.
The command `make install-toolchain` will download, build and install the
toolchain. The installation script can be configured using environment
variables. Use the command `./build-aux/install-toolchain.sh --help` to get
information about these environment variables.

After you have installed the cross toolchain and have adjusted your `PATH` so
that it contains the toolchain, you can type `make` to build Dennix.

## License

Dennix is free software and is released under the terms of the ISC license. The
full license terms can be found in the `LICENSE` file.
