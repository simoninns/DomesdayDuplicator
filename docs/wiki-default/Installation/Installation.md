# Installation of ld-decode (from source code)

# Supported Operating Systems

ld-decode and the various included tools are developed and tested on Ubuntu 22.04 LTS.  The tool-chain is a combination of python3 and Qt6/C++ applications providing the possibility of cross-platform deployment to many Linux flavours as well as non-OSS environments such as Windows and Apple MacOS.

It is possible to compile and run ld-decode in other Linux environments; however this is not regularly tested by the project. There is a community maintained [Linux Compatibility Document](https://docs.google.com/document/d/132ycIMMNvdKvrNZSzbckXVEPQVLTnH_YX0Oh3lqtkkQ/edit) available.

For first-time users using a pre-built binary build will be the most straight-forward way to get going.  Please see the section on pre-built binaries below.

# System requirements

ld-decode performs complex mathematics on huge datasets and therefore requires a fairly high-end PC for any expedient use, with AVX2 support notably helpful.

A Haswell (or newer) i9/i7 or Ryzen with 16-64Gb of RAM and 2TB of soild state & 8TB of hard-drive storage is recommended, however the minimum requirements are a Sandy Bridge i5 with 8Gb RAM and 512Gb of hard-drive.

Decoding in simple terms is single core bias, so faster higher speed integrated CPUs like those found in the Apple M1 Max, and AMDs x3D line and newer are today's fastest chips, the decoders today wont be more efficient past 6 threads. (excluding the chroma-decoder and hifi-decode)


# Compiling from source

For the most up-to-date instructions please see the build instructions found in [The main project repository](https://github.com/happycube/ld-decode)


# Pre-built binaries

If you are looking for pre-built binaries please see the page on [Windows Builds](Windows-Build.md) or [MacOS Builds](MacOS-Build.md)