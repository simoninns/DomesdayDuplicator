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

The CyFX3 SDK is included in this repository at `firmware/fx3-firmware/cyfx3sdk/`. No additional SDK installation is required.

## Building with CMake (Recommended)

### Quick Build

Navigate to the fx3-firmware directory and run:

```bash
cd firmware/fx3-firmware
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

To load the firmware onto the FX3 device, use the Cypress cyusb_linux tool (please see the Cypress documentation for installation instructions). The steps are as follows:

1. Close/short jumper J4 (PMODE) on the FX3 Superspeed board
2. Power off the FX3 and then power it on again
3. Load the cyusb_linux application
4. Highlight the FX3 bootloader device at the top of the window
5. Click on the 'program' tab
6. Select I2C EEPROM
7. Click on 'select file' and select the FX3 programming file from disk (use the `firmware.img` file from the build directory)
8. Click on 'Start download' to write the programming file to the device
9. Wait for programming to complete
10. Remove the jumper from J4
11. Power off the FX3 and then power it on again

## Troubleshooting

### arm-none-eabi-gcc not found

Ensure the ARM toolchain is installed and in your PATH:
```bash
which arm-none-eabi-gcc
```

If not found, install it or add it to your PATH.

### SDK not found

Verify the cyfx3sdk directory exists at:
```bash
ls -la ../cyfx3sdk/
```

The CMake build system expects the SDK to be located at `firmware/fx3-firmware/cyfx3sdk/`.

### Build Errors

Ensure all prerequisites are installed:
```bash
arm-none-eabi-gcc --version
cmake --version
```
