# Building the GUI Application

## Requirements

- **Qt 6.2 or later** (Qt 6.3+ recommended)
- **LibUSB 1.0**
- **C++20 compatible compiler**
- **CMake 3.16 or later**

## Build Instructions

### Using CMake (Recommended)

The application uses out-of-source builds with CMake. From the `gui/` directory:

```bash
cmake -B build
cmake --build build
```

This builds the capture application, `DomesdayDuplicator`.

#### Installation

After building, you can install the applications:

```bash
sudo cmake --install build
```

Pass `--prefix` to install somewhere other than `/usr/local`.

#### Version stamping

All three programs report the commit they were built from, through `--version` and their
About dialogs:

```
$ DomesdayDuplicator --version
DomesdayDuplicator 2.1 (a1b2c3d4)
```

The build asks `git` for the hash, but only as a fallback. Any build without a `.git` beside
it — Nix, a release tarball, a shallow CI clone — must pass the value in:

```bash
cmake -B build -DDDD_VERSION=a1b2c3d4
```

Letting it fall back to `unknown` produces a binary that cannot be traced to its source, so
the release workflow treats that as a failure rather than a warning.

### Editor and IDE support

There is only one build definition — the CMake one. The qmake `.pro` files that used to sit
alongside it have been removed: they were a second definition of the same build, maintained
by hand, and drifted from CMake.

Configuring the build writes `build/compile_commands.json`, and [.clangd](.clangd) points
clangd at it. Any editor with a language-server client then gets completion, navigation and
diagnostics with no further setup.

**Qt Creator** opens CMake projects natively — use *File → Open File or Project* and select
`CMakeLists.txt` rather than a `.pro` file.

**VS Code** users want the CMake Tools and clangd extensions. **CLion**, **KDevelop**, **vim**
and **Emacs** all read either the CMake project or `compile_commands.json` directly.

## Platform-Specific Notes

### Linux

Install dependencies using your package manager:

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential cmake qt6-base-dev libqt6serialport6-dev \
  libusb-1.0-0-dev libflac-dev libogg-dev
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtserialport-devel \
  libusb-devel flac-devel libogg-devel
```

### macOS

Install dependencies using Homebrew:

```bash
brew install qt@6 libusb flac cmake
```

### Windows

1. Install Qt from the official website
2. Install libusb (can use vcpkg or download pre-built binaries)
3. Use CMake with Visual Studio or MinGW

## Troubleshooting

- If CMake cannot find Qt, set the `CMAKE_PREFIX_PATH` to your Qt installation directory
- For libusb issues, ensure the library is in your system's library path
