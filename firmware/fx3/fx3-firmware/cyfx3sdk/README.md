# Vendored Cypress/Infineon FX3 SDK

This directory holds the subset of the EZ-USB FX3 SDK that the Domesday Duplicator firmware
build requires. It is vendored rather than fetched because the vendor download requires a
login, so no unattended fetch is possible.

## Provenance

| | |
| --- | --- |
| SDK version | 1.3.5 |
| Source archive | `ezusbfx3sdk_1.3.5_Linux_x32-x64.tar.gz` → inner `fx3_firmware_linux.tar.gz` → `cyfx3sdk/` |
| Archive date | 11 July 2023 |
| Verified | 12 August 2026 — every file here is byte-identical to the official archive |

## What is here, and why

The build fails with an explicit `FATAL_ERROR` if any of the first three are missing.
`CMakeLists.txt` derives all of these from `CYFX3SDK_PATH`, which defaults to this directory.

| Path | Purpose |
| --- | --- |
| `fw_lib/1_3_5/inc/` | 47 SDK headers — `CYFX3SDK_INCLUDE_DIR` |
| `fw_lib/1_3_5/fx3_release/` | Link libraries — `CYFX3SDK_LIB_DIR`. Only `libcyfxapi.a`, `libcyu3threadx.a` and `libcyu3lpp.a` are linked; the other four (`libcy_as0260.a`, `libcy_ov5640.a`, `libcyu3mipicsi.a`, `libcyu3sport.a` — image sensor, MIPI-CSI and serial port) are unused |
| `fw_build/fx3_fw/fx3.ld` | Linker script — `CYFX3SDK_LINKER_SCRIPT` |
| `util/elf2img/elf2img.c` | Host tool converting the `.elf` to a boot-loadable `.img` |
| `fw_lib/1_3_5/fx3_debug/`, `fx3_profile_debug/`, `fx3_profile_release/` | Unused debug and profiling library variants, ~45 MB |

The rest of the SDK — Eclipse, the ARM GCC toolchain, `boot_lib`, `firmware` examples, `doc`,
`JTAG` and the PDFs — is deliberately **not** vendored. The toolchain comes from nixpkgs
(`gcc-arm-embedded`) and the project does not depend on an IDE.

## Two things to preserve when refreshing this directory

1. **`util/elf2img/CMakeLists.txt` is project-authored, not vendor.** The official archive
   ships only `elf2img.c` and `readme.txt`. That CMake file was added in commit `19bdb88` and
   is what builds the host tool. Copying the vendor's `util/elf2img/` over the top deletes it
   and breaks the firmware build.
2. **The version is embedded in the path.** `CMakeLists.txt` sets
   `CYFX3SDK_VERSION "1_3_5"`, so the library directory must be literally `fw_lib/1_3_5/`.
   A different SDK version means renaming the directory *or* updating that variable — never
   leaving the two disagreeing.

## Licensing

The official Linux archive contains **no licence file** anywhere in the `cyfx3sdk` tree —
only three PDFs at its root. The headers here refer to a licence at
`<install>/license/license.txt`, which the vendor's installer generates rather than shipping
in the tarball, so there is no such file to vendor alongside them.

The headers themselves carry Cypress's standard notice: *Copyright Cypress Semiconductor
Corporation 2010-2023, All Rights Reserved; UNPUBLISHED, LICENSED SOFTWARE; CONFIDENTIAL AND
PROPRIETARY INFORMATION*.

Vendoring this SDK is a deliberate project decision recorded in
[`docs-tech/decisions.md`](../../../../docs-tech/decisions.md) (P0-2), taken on the basis
that the SDK is already widely mirrored.

Note the separate host-tools component, `cyusb_linux`, **is** LGPL-2.1 licensed — see
`../../fx3-programmer/LICENSE.cyusb_linux.txt`.
