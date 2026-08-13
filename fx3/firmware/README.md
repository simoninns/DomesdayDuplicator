# Building the Domesday Duplicator FX3 Firmware

## Quick build with Nix

Run this from anywhere in the working tree — Nix walks up to the root flake:

```bash
nix build .#fx3-firmware
ls result/            # firmware.img  firmware.elf  firmware.map
```

Nothing has to be installed first: the cross toolchain, `fx3-mkimage` and the vendored SDK
all come from the flake. The build also runs the descriptor golden test, and the version stamp
comes from the flake's own revision rather than from `git` (see *Version stamping* below).

For an interactive shell with the same toolchain, `nix develop .#fx3`.

**Nix is a convenience, not a requirement.** The rest of this document is the toolchain-only
route, and it must keep working.

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

| Variable | Default | Purpose |
| --- | --- | --- |
| `CYFX3SDK_PATH` | `../sdk` | Where the vendored CyFX3 SDK lives |
| `FIRMWARE_VERSION` | `git rev-parse --short=8 HEAD`, else `unknown` | The commit stamped into the USB product descriptor |
| `FIRMWARE_INSTALL_DIR` | `bin` | Directory under the install prefix for the three artefacts |
| `BUILD_TESTING` | `ON` at top level | Build and register the test suite |

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake \
      -DCYFX3SDK_PATH=/path/to/custom/sdk \
      ..
```

### Version stamping

The firmware carries the commit it was built from, and it is the only artefact in this
project that can be identified *while running* — the string reaches the USB product
descriptor, so `lsusb -v` on a device reports it:

```
iProduct  2  Domesday Duplicator (a1b2c3d4)
```

The build asks `git` for the hash, but only as a fallback. Any build without a `.git` beside
it — Nix, a release tarball, a shallow CI clone — must pass the value in:

```bash
cmake -DFIRMWARE_VERSION=a1b2c3d4 ...
```

Letting it fall back to `unknown` produces an image that cannot be traced to its source, so
the release workflow treats that as a failure rather than a warning.

### Tests

```bash
ctest --test-dir build
```

One test today: a golden comparison of the generated USB descriptor against committed
reference headers, covering the byte layout and the computed length byte. There is no unit
tier and there cannot usefully be one — every source file here is freestanding ARM926EJ-S
code calling into the Cypress SDK, so the build host cannot execute any of it. See
[TESTING.md](../../TESTING.md).

If you change `generate-descriptor.sh` deliberately, regenerate the references:

```bash
for c in 0123abcd unknown; do
    bash generate-descriptor.sh /tmp "$c" > "tests/descriptor-$c.h"
done
```

### fx3-mkimage

`fx3-mkimage` converts the linked `.elf` into the boot-loadable `.img`. It is a *host* tool,
so it cannot be built by this project's ARM toolchain. The build prefers one already on
`PATH` — which is what `nix develop .#fx3` and `nix build .#fx3-firmware` provide — and
otherwise compiles it from `../mkimage/src/` with a host compiler in a single step. Either
way the configure output says which route it took, so a plain `cmake` build needs nothing
installed.

It replaces the Cypress SDK's `elf2img`, which was deleted from the repository in Phase 5.
`fx3-mkimage` is the project's own GPLv3 code, written against Infineon's public application
note AN76405 and producing byte-identical output — see
[`../mkimage/README.md`](../mkimage/README.md).

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
