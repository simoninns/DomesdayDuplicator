# Vendored Cypress/Infineon FX3 SDK

This directory holds the subset of the EZ-USB FX3 SDK that the Domesday Duplicator firmware
build requires. It is vendored rather than fetched because the vendor download requires a
login, so no unattended fetch is possible.

## Provenance

| | |
| --- | --- |
| SDK version | 1.3.5 |
| Vendor | Infineon (formerly Cypress Semiconductor) |
| Product | EZ-USB™ FX3 Software Development Kit |
| Origin | [softwaretools.infineon.com/tools/com.ifx.tb.tool.ezusbfx3sdk](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.ezusbfx3sdk) — an Infineon account is required, which is why there is no unattended fetch and no `fetchurl` derivation |
| Source archive | `ezusbfx3sdk_1.3.5_Linux_x32-x64.tar.gz` (445 MB) → inner `fx3_firmware_linux.tar.gz` → `cyfx3sdk/` |
| Archive date | 11 July 2023 |
| Refreshed here | 12 August 2026 |
| Verified | 12 August 2026 — every file here is byte-identical to the official archive |

There is no stable direct download URL to record: the file is served from Infineon's gated
software portal behind a login. That is the whole reason this directory exists rather than a
`pkgs.fetchurl`. Nothing in nixpkgs packages the parts that matter either — the ARM libraries,
the headers, and the `fx3.ld` linker script — and that absence is structural rather than an
oversight, since nixpkgs has no URL it can fetch unattended and does not vendor
multi-megabyte binary blobs. Vendoring in-tree is the remaining mechanism that is both
reproducible and cacheable.

## What is here, and why

The build fails with an explicit `FATAL_ERROR` if any of the first three are missing.
`CMakeLists.txt` derives all of these from `CYFX3SDK_PATH`, which defaults to this directory.

| Path | Purpose |
| --- | --- |
| `fw_lib/1_3_5/inc/` | 47 SDK headers — `CYFX3SDK_INCLUDE_DIR` |
| `fw_lib/1_3_5/fx3_release/` | Link libraries — `CYFX3SDK_LIB_DIR`. Exactly the three that are linked: `libcyfxapi.a`, `libcyu3threadx.a`, `libcyu3lpp.a` |
| `fw_build/fx3_fw/fx3.ld` | Linker script — `CYFX3SDK_LINKER_SCRIPT` |

Two rounds of pruning have already happened here. **Do not re-extract any of it** when
refreshing this directory:

- The `fx3_debug/`, `fx3_profile_debug/` and `fx3_profile_release/` library variants —
  around 45 MB, deleted in Phase 2, since `CMakeLists.txt` only ever links `fx3_release`.
- Four unused archives from `fx3_release/` itself — `libcy_as0260.a`, `libcy_ov5640.a`,
  `libcyu3mipicsi.a` and `libcyu3sport.a` (image sensor, MIPI-CSI and serial port), around
  1.8 MB, deleted in Phase 5. Removing them changed the linked firmware not at all: the
  boot image is byte-for-byte the same.
- `util/elf2img/`, the vendor's ELF-to-boot-image tool, **replaced and deleted in Phase 5**.
  See below.

The rest of the SDK — Eclipse, the ARM GCC toolchain, `boot_lib`, `firmware` examples, `doc`,
`JTAG` and the PDFs — is deliberately **not** vendored. The toolchain comes from nixpkgs
(`gcc-arm-embedded`) and the project does not depend on an IDE.

## How the Nix build reaches this directory

`fx3/firmware/package.nix` does **not** hand the whole directory to the firmware
derivation. It passes a `lib.fileset` narrowed to exactly three paths —
`fw_lib/1_3_5/inc`, `fw_lib/1_3_5/fx3_release` and `fw_build/fx3_fw/fx3.ld` — as
`CYFX3SDK_PATH`, so editing this README does not trigger a firmware rebuild and the store
path stays small.

**The consequence for a refresh:** if a future SDK version needs a file outside those three
paths, adding it here is not enough — the fileset in `package.nix` has to name it too, or
the Nix build will fail with a `FATAL_ERROR` that a plain `cmake` build does not produce.

## The vendor's elf2img is gone, and must not come back

The SDK's `util/elf2img/` was replaced in Phase 5 by
[`../mkimage`](../mkimage) — `fx3-mkimage`, a from-scratch GPLv3 implementation written
against Infineon's public application note AN76405, producing byte-identical output. The
vendored copy has been deleted.

**When refreshing this directory, do not restore `util/`.** It is not needed by any build,
and re-adding it would put a proprietary artefact back into the firmware build path for no
gain. `../mkimage/README.md` records the format reference and the byte-identity check.

## Two things to preserve when refreshing this directory

1. **Extract only the three paths listed above.** Anything else is checkout weight the build
   never opens, and `util/` in particular must stay out (see above).
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

Vendoring this SDK is a deliberate project decision, taken on 12 August 2026 on the basis
that the SDK is already widely mirrored in public GitHub repositories, and that the licence
agreement the headers point at is not obtainable — so the position cannot be verified either
way from the material the vendor ships.

Note the separate host-tools component, `cyusb_linux`, **is** LGPL-2.1 licensed — see
`../programmer/LICENSE.cyusb_linux.txt`.

### How this is expressed in Nix

**Nothing in this directory carries a `meta.license` any more, because nothing in it is a
package.** What remains here — headers, three static archives and a linker script — is
consumed as a `lib.fileset` input to the firmware derivation, not built as a derivation of
its own.

That was not true before Phase 5. The vendored `elf2img` *was* a package, and it needed a
licence attribute that could describe proprietary vendor code without making
`nix build .#fx3-firmware` fail for every user lacking `allowUnfree`. The stopgap was a named
custom licence with `free = true` — a project decision, not a legal determination. Replacing
the tool removed the need for it, and it is gone.

The firmware derivation itself declares `gpl3Plus`, which describes the project's own sources.
That it links against the archives here is a fact this file records; the P0-2 decision to
vendor them stands unchanged.
