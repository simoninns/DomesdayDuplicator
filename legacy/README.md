# The legacy image set

The original Domesday Duplicator firmware and gateware, frozen. These four files are the
only binaries in this repository that are **not** built by CI, and that is deliberate:
they are the record of a state the project has moved on from, and rebuilding them would
mean keeping a tree from before the update mechanism buildable forever.

They exist so that **Tools ▸ Firmware ▸ Legacy ▸ Roll back to legacy firmware…** has
something to install — a unit can be returned to the software it shipped with, and brought
back again, on the same bench, as often as anybody likes. That loop is also how the
bring-up flow is tested against a genuinely legacy board without hoarding
never-updated hardware.

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `firmware.img` | 111,748 | `b33cde2abaf7eeceb1cd34a451f0288cc1c84fad6539d68e61af1b84034226af` |
| `gateware.rpd` | 215,200 | `f5a1caad686d2c143ea8800188efe348a5791d2df957c59c204595e9350934d1` |
| `gateware.sof` | 704,015 | `d07acdfffd8879bedf70a15fa499efed4dac5dafac2bee1bf0cb4c921c81c1bf` |
| `gateware.cof` | 1,076 | `fb59184f8d593197dd9f5ccefe1205a4f3e88b5b195a9f28260f892591ac31c2` |

## Where they came from

| | |
| --- | --- |
| Pinned ref | **`bb65470`** — `97f7dec^`, the last commit whose firmware and gateware present the legacy USB identity `1d50:603b` |
| Generated | 2026-08-17, task **B-V2** in [TESTING.md](../TESTING.md) |
| Toolchain | `arm-none-eabi-gcc` from that tree's own flake lock; Quartus Prime Lite 25.1std.0 Build 1129 |
| Built by | `nix build` against the pinned tree, with the commit passed in explicitly so the firmware's USB product string reads *Commit: bb65470* rather than *unknown* |

`97f7dec` is the commit that moved the VID/PID to the pid.codes-assigned `1209:2347`. Its
parent is therefore the last tree that *is* a legacy Duplicator — and, decisively, that
tree is already the modern monorepo, so it builds with today's tooling. It predates the
SPI register interface, the update agent and the factory/application flash split, which is
exactly what makes it the right thing to roll back to.

**These files are frozen. Nothing rebuilds them and no workflow checks out that tree.**
If they ever have to be regenerated, the procedure is B-V2 in TESTING.md; the digests above
are what a regeneration has to reproduce.

## Why a `.rpd` and not a `.jic`

A rollback writes the legacy image to EPCS address `0x000000` through the **firmware's own
flash bridge**, over USB — the same path a gateware update takes, aimed at the factory
region instead of the application one. That path writes raw bytes, so raw bytes are what is
committed.

The `.jic` a legacy build produces is 8.4 MB of mostly padding and is not committed. It is
derivable from `gateware.sof` and the legacy `.cof`, and its digest is
`95480a5fc4530e2b51d08211ce956705df653434169b80a3a87b7673213fcf29` if a bench ever wants to
hand one to `quartus_pgm`.

`gateware.cof` is committed for the same reason the `.sof` is: it is what turns one into the
other. It is the legacy tree's own `.cof` with two settings changed, and the second one
matters:

| Setting | Legacy tree | Here | Why |
| --- | --- | --- | --- |
| `auto_create_rpd` | 0 | **1** | the legacy build had no reason to emit raw bytes; a rollback needs them |
| `rpd_little_endian` | 1 | **0** | the configuration engine consumes the stream LSB-first while SPI delivers MSB-first, so the flash must hold each byte bit-reversed. `quartus_pgm` performs that reversal when it programs a `.jic`; the update path writes the `.rpd` verbatim, so the file has to arrive already reversed. See the note in `fpga/application/DomesdayDuplicator.cof` for the day of bench time this cost |

The image content is identical either way. What differs is the order the bytes are recorded
in, and which of the two is right depends only on what writes them.

## What a rolled-back unit is

`1d50:603b` on the bus, a single gateware image at address `0`, and no way to update itself
— the legacy firmware predates the update protocol entirely. `ddd-gui` recognises such a
device and says so, but cannot drive it; the legacy capture application is what it is for.

Bring-up puts it back. Nothing here is one-way.
