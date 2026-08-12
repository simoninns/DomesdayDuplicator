# Building the GUI Application

## Requirements

- **Qt 6.2 or later** (Qt 6.3+ recommended)
- **LibUSB 1.0**
- **C++20 compatible compiler**
- **CMake 3.16 or later**

## Build Instructions

### Using CMake (Recommended)

The application uses out-of-source builds with CMake. Build from the repository root:

```bash
mkdir build && cd build
cmake ..
make
```

#### Installation

After building, you can install the applications:

```bash
sudo make install
```

### Using Qt Creator

Alternatively, you can open the individual `.pro` files in the `tools/` directory using Qt Creator:

1. Open Qt Creator
2. Open one of the project files:
   - `tools/DomesdayDuplicator/DomesdayDuplicator.pro` - Main capture application
   - `tools/dddutil/dddutil.pro` - Data analysis utility
   - `tools/dddconv/dddconv.pro` - Command-line conversion tool
3. Configure the project with your preferred kit
4. Build and run

## Platform-Specific Notes

### Linux

Install dependencies using your package manager:

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential cmake qt6-base-dev libusb-1.0-0-dev
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel libusb-devel
```

### macOS

Install dependencies using Homebrew:

```bash
brew install qt@6 libusb cmake
```

### Windows

1. Install Qt from the official website
2. Install libusb (can use vcpkg or download pre-built binaries)
3. Use CMake with Visual Studio or MinGW

## Troubleshooting

- If CMake cannot find Qt, set the `CMAKE_PREFIX_PATH` to your Qt installation directory
- For libusb issues, ensure the library is in your system's library path
