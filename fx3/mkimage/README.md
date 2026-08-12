# fx3-mkimage

Converts the linked FX3 firmware `.elf` into the boot-loadable `.img` that the Cypress FX3
bootloader expects.

```bash
fx3-mkimage -i firmware.elf -o firmware.img -v
```

It is a **host** tool. The firmware build has to *run* it, so it can never be built with the
firmware's ARM cross toolchain — which is why it is a component of its own rather than a
target inside `../firmware`.

## Why this exists

It replaces `elf2img`, the equivalent utility that used to be vendored with the Cypress FX3
SDK at `fx3/sdk/util/elf2img/`. That file carried Cypress's proprietary header
(*UNPUBLISHED... CONFIDENTIAL AND PROPRIETARY*), which meant the firmware build could not be
described honestly in packaging metadata: no licence in `lib.licenses` fitted it, and marking
it unfree would have made `nix build .#fx3-firmware` fail for everyone without
`allowUnfree` — see the Phase 5 notes in
[`docs-tech/implementation-plan.md`](../../docs-tech/implementation-plan.md).

This implementation is **GPLv3, written from a public specification**, and the vendored
`elf2img` has been deleted from the repository. That removes the smaller of the two
proprietary artefacts from the firmware build path.

**It does not make the project SDK-free.** The ARM libraries, the 47 headers and `fx3.ld`
in [`../sdk`](../sdk) are still required to build the firmware, and they are the binding
constraint. This is a contained improvement, not a licence liberation.

## Where the format comes from

Infineon application note **AN76405, "EZ-USB™ FX3/FX3S boot options"**, section 4.4 (*Boot
image format*, Table 14) and its worked example in 4.4.1. That note is public and needs no
login:

<https://www.infineon.com/dgdl/Infineon-AN76405_-EZ-USB_FX3_FX3S_boot_options-ApplicationNotes-v12_00-EN.pdf>

The SDK's own `readme.txt` is **not** a sufficient source — it documents the command-line
options and the meaning of the EEPROM control byte, but contains no binary layout at all: no
signature, no section structure, no checksum. It is a user manual.

### The format

| Field | Size | Content |
| --- | --- | --- |
| `wSignature` | 2 | `'C'`, `'Y'` |
| `bImageCTL` | 1 | Bit 0 clear = executable image. Bits 3:1 EEPROM size, bits 5:4 bus speed. Default `0x1C` |
| `bImageType` | 1 | `0xB0` = normal firmware with checksum |
| `dLength` | 4 | Section length in 32-bit **words** |
| `dAddress` | 4 | Section load address, 32-bit aligned |
| `dData` | 4×`dLength` | Section payload |
| … | | more sections |
| `dLength N` | 4 | `0x00000000` — termination record |
| `dAddress N` | 4 | Program entry point |
| `dCheckSum` | 4 | 32-bit sum of the section payload words only |

AN76405 is explicit about the checksum's range: *"The checksum will not include the dLength,
dAddress, and Image Header."* `tests/test_bootimage.cpp` asserts the tool reproduces the
note's own worked example, `0x6AF37AF2`.

## Three behaviours that are not in the specification

The specification defines the container. Turning an ELF into one involves three decisions
AN76405 does not make, all determined empirically from the firmware image the vendor tool
produced and all required for byte-identical output:

1. **The 0x00–0x100 vector area is dropped by default.** The ARM926EJ-S keeps its reset and
   interrupt vectors there and the firmware copies them in itself once running, so loading
   over that range would overwrite the live bootloader. `-vectorload yes` retains it. (This
   one *is* described in the SDK readme, just not in the app note.)
2. **Sections are split at 64 KiB.** AN76405 places no limit on a section's length; this is
   what the vendor tool does. A firmware segment larger than 0x10000 bytes becomes several
   consecutive sections.
3. **`p_memsz` is used, not `p_filesz`, and the difference is zero-filled.** That difference
   is `.bss`, so the image carries it pre-zeroed rather than leaving it to startup code.

## Acceptance: byte-identical output

The replacement was accepted on a straight byte comparison against the tool it replaces,
using the project's own firmware:

```
$ sha256sum firmware.img          # built with the vendored Cypress elf2img
4938a7d10927285128285f70f17ba4ca5edde1bd329aac313eda80622a54bcd4
$ sha256sum firmware.img          # built with fx3-mkimage
4938a7d10927285128285f70f17ba4ca5edde1bd329aac313eda80622a54bcd4
```

111,316 bytes, 4 sections, identical. That comparison **cannot be re-run** — the vendor tool
is no longer in the tree, which is the point. What guards the format from here on is the
golden byte vectors in `tests/`, and ultimately a device that boots.

## Building and testing

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

32 tests, tiers T1 and T2. Everything worth testing is pure — ELF buffer in, image buffer out
— which is why `src/fx3-bootimage.c` holds the logic and `src/main.c` holds all the file I/O.

The firmware build finds this tool with `find_program(FX3_MKIMAGE fx3-mkimage)`. It is on
`PATH` in `nix develop .#fx3` and in `nix build .#fx3-firmware`; outside those, the firmware's
`CMakeLists.txt` falls back to compiling the two source files here with a host compiler, so a
plain `cmake` build still works with nothing installed.

## Licensing

GPLv3, like the rest of the project's own software. It is not derived from the Cypress tool:
it was written against AN76405 and against the ELF specification. `git log` on this directory
is the record.
