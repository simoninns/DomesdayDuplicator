# Building from source

Building the capture application yourself is for development, and for platforms the
released packages do not cover. If you only want to run it, take a
[package](index.md) instead — they carry their own dependencies and need no toolchain.

This page covers the capture application. To build the FPGA gateware or the FX3 firmware as
well, see [Building Locally](../development/building-locally.md).

## With Nix

The reproducible route, and the one CI uses. Everything is pinned by the repository's single
`flake.lock`, so the build you get is the build everyone else gets.

```bash
git clone https://github.com/simoninns/DomesdayDuplicator
cd DomesdayDuplicator

nix build .#gui           # build it
./result/bin/DomesdayDuplicator

nix develop .#gui         # or get a shell with the toolchain and editor tooling
```

Inside the development shell:

```bash
cmake -B build -S gui -G Ninja
cmake --build build
ctest --test-dir build            # the T1/T2 suite
```

`nix develop .#gui` works from any directory in the tree — Nix walks up to find the flake at
the repository root.

## Without Nix

These are the same toolchains the packaging jobs use, which is what keeps this page honest:
if one of these dependency lists is wrong, the corresponding installer stops building.

### Ubuntu and Debian

```bash
sudo apt install --no-install-recommends \
  git cmake ninja-build pkg-config libgl-dev build-essential \
  qt6-base-dev libqt6serialport6-dev qt6-tools-dev \
  libusb-1.0-0-dev libflac-dev libogg-dev libgtest-dev

cmake -B build -S gui -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary lands at `build/bin/DomesdayDuplicator`.

### macOS

```bash
brew install cmake ninja pkg-config qt@6 libusb flac googletest

export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake -B build -S gui -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows

Use the MSYS2 UCRT64 environment:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-serialport \
  mingw-w64-ucrt-x86_64-qt6-tools mingw-w64-ucrt-x86_64-libusb \
  mingw-w64-ucrt-x86_64-flac mingw-w64-ucrt-x86_64-libogg \
  mingw-w64-ucrt-x86_64-gtest

cmake -B build -S gui -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Running the result outside the MSYS2 shell needs the Qt and libFLAC DLLs beside it —
`windeployqt` collects the Qt ones. The MSI does the rest of that work for you, which is
rather the point of it.

## Stamping the version

A build from a git checkout takes its version from `git rev-parse`. A build from a tarball,
or inside a sandbox with no `.git`, has nothing to ask, so pass it explicitly:

```bash
cmake -B build -S gui -DDDD_VERSION=1a2b3c4d
```

Without it the application reports its version as `unknown`, which is refused by the release
workflow — a released binary that cannot be traced back to a commit is the thing that
version stamping exists to prevent.

## Building the installers

The packaging definitions live under `gui/packaging/`:

| Path | What it builds |
| --- | --- |
| `gui/packaging/flatpak/` | the Flatpak manifest |
| `gui/packaging/windows/` | the WiX installer definition |
| `gui/packaging/assets/` | the `.desktop` file, AppStream metadata and icons shared by all three |

The workflows that drive them are `.github/workflows/package-*.yml`, and each can be read as
the exact recipe for building that package by hand.
