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

**Nix on Linux is the only supported build environment**, here as everywhere else in this
repository. Everything below runs inside `nix develop .#fx3`; do not add a per-distribution
dependency list or a second build route.

### The CyFX3 SDK

The CyFX3 SDK is vendored in this repository at `../sdk/`, and the flake puts it where the
build expects it. No SDK installation is required.

## Building with CMake

For an editing loop rather than a one-shot `nix build`. Run it from the repository root and
build **out of tree** — never inside `fx3/firmware`. The toolchain file must be an
**absolute** path, because CMake resolves a relative one against the build directory:

```bash
nix develop .#fx3

cmake -B build/fx3-firmware -S fx3/firmware -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/fx3/firmware/arm-none-eabi-toolchain.cmake"
cmake --build build/fx3-firmware
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
cmake -B build/fx3-firmware -S fx3/firmware -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/fx3/firmware/arm-none-eabi-toolchain.cmake" \
      -DCYFX3SDK_PATH=/path/to/custom/sdk
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
ctest --test-dir build/fx3-firmware
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
way the configure output says which route it took.

It replaces the Cypress SDK's `elf2img`, which has been deleted from the repository.
`fx3-mkimage` is the project's own GPLv3 code, written against Infineon's public application
note AN76405 and producing byte-identical output — see
[`../mkimage/README.md`](../mkimage/README.md).

### Clean Build

```bash
rm -rf build/fx3-firmware
```

## Programming the FX3

To load the firmware onto the FX3 device, use the `fx3-programmer` tool included in this repository. Please see `../programmer/README.md` for detailed programming instructions.

## Troubleshooting

### arm-none-eabi-gcc not found

You are not in the development shell. Enter it with `nix develop .#fx3` and check:
```bash
which arm-none-eabi-gcc
```

### SDK not found

Verify the SDK directory exists at:
```bash
ls -la ../sdk/
```

The CMake build system expects the SDK at `../sdk/`. Override with `-DCYFX3SDK_PATH=...` if it is elsewhere.

### Build Errors

Confirm you are in the development shell, which is where every tool the build needs comes
from:
```bash
arm-none-eabi-gcc --version
cmake --version
```
