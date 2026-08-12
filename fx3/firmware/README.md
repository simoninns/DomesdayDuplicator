# Building the Domesday Duplicator FX3 Firmware

## Prerequisites

### Required Tools

1. **ARM GCC Toolchain** - Install the ARM bare-metal toolchain:
   
   **On Ubuntu/Debian:**
   ```bash
   sudo apt-get install gcc-arm-none-eabi
   ```
   
   **On Fedora/RHEL:**
   ```bash
   sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-newlib
   ```

2. **CMake** - Version 3.10 or later:
   
   **On Ubuntu/Debian:**
   ```bash
   sudo apt-get install cmake
   ```
   
   **On Fedora/RHEL:**
   ```bash
   sudo dnf install cmake
   ```

3. **Build Tools**:
   
   **On Ubuntu/Debian:**
   ```bash
   sudo apt-get install build-essential
   ```
   
   **On Fedora/RHEL:**
   ```bash
   sudo dnf groupinstall "Development Tools"
   ```

4. **32-bit Libraries** (if on 64-bit system):
   
   **On Ubuntu/Debian:**
   ```bash
   sudo apt-get install lib32z1
   ```
   
   **On Fedora/RHEL:**
   ```bash
   sudo dnf install glibc.i686
   ```

### CyFX3 SDK

The CyFX3 SDK is included in this repository at `../sdk/`. No additional SDK installation is required.

## Building with CMake (Recommended)

### Quick Build

From the repository root:

```bash
cd fx3/firmware
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake ..
cmake --build .
```

### Build Output

The build process will generate:
- `firmware.elf` - The executable ELF file
- `firmware.img` - The boot-loadable binary image for the FX3
- `firmware.map` - Memory map file

### Build Options

You can customize the SDK path if needed:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake \
      -DCYFX3SDK_PATH=/path/to/custom/sdk \
      ..
```

### Clean Build

To clean the build directory:
```bash
cd build
rm -rf *
```

## Programming the FX3

To load the firmware onto the FX3 device, use the `fx3-programmer` tool included in this repository. Please see `../programmer/README.md` for detailed programming instructions.

## Troubleshooting

### arm-none-eabi-gcc not found

Ensure the ARM toolchain is installed and in your PATH:
```bash
which arm-none-eabi-gcc
```

If not found, install it or add it to your PATH.

### SDK not found

Verify the SDK directory exists at:
```bash
ls -la ../sdk/
```

The CMake build system expects the SDK at `../sdk/`. Override with `-DCYFX3SDK_PATH=...` if it is elsewhere.

### Build Errors

Ensure all prerequisites are installed:
```bash
arm-none-eabi-gcc --version
cmake --version
```
