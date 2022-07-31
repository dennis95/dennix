# Dennix

Dennix is a unix-like hobbyist operating system for x86 and x86_64 that has
been in development by a single developer since 2016. Exciting features
include:

- A monolithic kernel written in C++
- A standard C library that is sufficiently complete to allow running most
  ports with no substantial modifications
- A reasonably complete shell
- Common command line utilities
- A graphical user interface
- Harddisk drivers for ATA and AHCI
- An ext2 file system driver
- A collection of ports of third-party software

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

## Ports

Patches and scripts for installing third-party ports are available at
<https://github.com/dennis95/dennix-ports>. If you put the contents of that
repository into a subdirectory named `ports` all third-party ports will be
downloaded, built, and installed automatically during the build. For releases a
ports tarball is available that includes all third-party source code and that
does not need to download any additional files.

## License

Dennix is free software and is licensed under the terms of the ISC license. The
full license terms can be found in the `LICENSE` file. The math library (libm)
code was adopted from musl and is licensed under the MIT license and other
permissive licenses compatible to the ISC license. See the `libm/COPYRIGHT`
file for details.

All third-party ports are released under their own licenses. The full license
text for every port is available in the `/share/licenses` directory of the
release image.

## Screenshots

![Screenshot showing the gui with the bricks game and the calculator.](https://user-images.githubusercontent.com/11199027/137330982-4a310a5e-e88a-42a0-83aa-90933e072fdc.png)

![Screenshot showing the gui with the doom port and compilation of a simple program that shows a window saying "Hello World!".](https://user-images.githubusercontent.com/11199027/137330998-2685b189-3777-44e2-b779-4440b6e8c7d3.png)

![Screenshot showing the compilation of utilities.](https://user-images.githubusercontent.com/11199027/137336790-28a27b1c-2d09-4a95-bcf0-763f3f7c491f.png)
