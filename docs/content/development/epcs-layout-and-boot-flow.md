# EPCS layout and boot flow

How the DE0-Nano's configuration flash is divided, which half of it can be updated in the field, and what happens at power-on.

!!! note "Specification, not yet a description"

    This page is **normative** and describes gateware that does not exist yet. Today `fpga/src/` holds one image, the EPCS holds one image, and nothing reads a boot block. This page defines what the two-image layout must be, so that the factory image — the one component a field update can never fix — is designed on paper and reviewed before it is frozen into hardware.

    The numbers marked as pending measurement are pending measurement.

## Two images, and why the resident one is tiny

The EPCS64 is 8 MB and the gateware needs about 350 KB of it, so there is room for a resident image that can never be field-updated and an application image that can. That is the whole safety story for gateware updates: there is no way to avoid writing flash — the FPGA has no other configuration source, and the host cannot reach its configuration pins at all — so instead the unit must always have something valid to fall back to.

The obvious design is for the resident image to be the full capture gateware, so that a unit that fell back still captured. That is the wrong shape, and the reason is one word: **frozen**.

A resident image containing the capture logic would change every time the capture logic changed — which is the opposite of resident. Either it would be updated in the field, and stop being the thing that cannot break, or it would be left alone and every fielded unit would carry a copy of whatever the capture path looked like on the day it was provisioned, silently ready to run it.

So the factory image is a **boot loader in the honest sense**: the smallest gateware that can identify itself, give the FX3 access to the flash, and decide whether to jump to the application image. It does not capture. A unit that falls back to it shows "recovery gateware running — reinstall gateware" with a one-click repair, which is a better failure mode than silently running stale capture logic against a disc somebody is trying to preserve.

| | factory | application |
| --- | --- | --- |
| PLL, safe idle drive of the GPIF pins | yes — `USB_PCLK` running, `dataAvailable` low | yes, full GPIF |
| `spiRegisters` — identity, commit, map version | yes | yes |
| `IMAGE_ROLE` at `0x0B` | `0x00` | `0x01` |
| Flash bridge — unlock and EPCS pass-through | yes | yes |
| Reconfiguration control | boot logic and trigger | tickle and trigger |
| Capture path — ADC, FIFO, GPIF state machine, test generator | **no** | yes |

The factory image keeps the register interface and the flash bridge because those are what make recovery possible from the application. It drives the GPIF pins to a safe idle rather than leaving them floating, because the FX3 is running on the other side of those pins whatever the FPGA is doing.

The application image carries the bridge too. Routine gateware updates are done *from the running application image*; the factory image is exercised only when something has gone wrong, and once per power-on at the moment of handover.

## The repository layout

The boundary is physical, so that a change to the frozen half is loud:

```
fpga/
  factory/      the static boot-loader image. Own Quartus project, own README
                stating the freeze policy. Compiled in remote-update "factory" mode.
                Expected to change ~never after its first release.
  application/  the flashable capture gateware. Own Quartus project, compiled in
                remote-update "application" mode. Changes freely; this is what the
                bundle carries.
  common/       Verilog shared by both: the spiRegisters core, the flash bridge, the
                asmiblock and reconfiguration wrappers and their simulation stubs,
                version generation.
```

A change under `common/` rebuilds both images, so it inherits the factory image's scrutiny. That is a cost and it is the right one: the alternative is two copies of the register core drifting apart, which would be a protocol split inside one device.

## Freeze policy

**The factory image is written by JTAG at provisioning time and never in the field.**

That is enforced by consequence rather than by a lock. Changing it means re-provisioning every fielded unit with a cable and a copy of Quartus, so after its first release it is effectively immutable — not because something prevents a change, but because a change is an operation nobody can perform remotely on hardware that is already in somebody's house.

Which means the factory image gets review and bench soak out of all proportion to its size. It is the one component that a field update can never repair, and the whole recovery story rests on it being correct.

## Layout

```
0x000000  factory image          JTAG-provisioned once; never field-written
0x100000  boot block             one 64 KiB sector
0x200000  application image      field-written by the update flow
0x400000  free                   reserved: a second application slot, for A/B
```

Addresses are sector-aligned with generous gaps. The gaps are not an estimate of how large the images might grow; they are there so that a change in image size never moves anything, because moving the boot block would mean re-provisioning every unit.

The boot block gets a **whole 64 KiB sector to itself** and holds a few dozen bytes. Erasing flash erases a whole sector, so anything sharing that sector would be destroyed and rewritten every time the boot block changed — and the boot block changes at exactly the moment when losing something else would be least recoverable.

### Boot block format

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Magic, `0x44 0x44 0x42 0x42` (`DDBB`) |
| 4 | 2 | Layout version, little-endian; `1` for this document |
| 6 | 2 | Reserved, zero |
| 8 | 4 | Application image start address |
| 12 | 4 | Application image length in bytes |
| 16 | 4 | CRC32 of the application image |
| 20 | 4 | CRC32 of bytes 0 to 19 of this block |

Everything after byte 24 in the sector is erased flash, `0xFF`.

The block carries its own checksum as well as the image's, so a boot block that was half-written — power lost mid-sector — is distinguishable from a boot block that is intact and points at a damaged image. Both mean "stay in factory", but they mean different things to whoever is diagnosing it.

The application's start address is stored rather than assumed, so that the future second slot needs no change to the factory image's logic. That is the one piece of forward compatibility the frozen image gets, and it is cheap: it is a field it reads instead of a constant it holds.

**CRC32 rather than SHA-256, and this is the only place in the update chain where that is true.** The check runs in the factory image's fabric, where a SHA-256 core cannot be justified in an image whose entire purpose is to be small and never change. It defends against *corruption only* — and it only has to. Authenticity was settled before the boot block was ever written: the bundle's signature, the digest checked on the way into the device, and the digest recomputed from the flash after the write. An attacker who could write the boot block could write the application image too, and no digest in the factory image would help. The trade-off is stated here rather than left to be discovered, because a lone CRC in a document full of SHA-256 looks like an oversight until you know why it is not.

## Boot flow

```
power on
   │
   ▼
FPGA loads the factory image from 0x000000     (hardware, always)
   │
   ▼
factory boot logic reads the boot block through the flash bridge
   │
   ├── magic wrong, layout version unknown, or either CRC bad
   │        └──▶ stay in factory. The unit is in recovery.
   │
   └── valid
            ├──▶ arm the remote-update watchdog
            └──▶ trigger reconfiguration from the application start address
                     │
                     ├── configuration CRC fails
                     │        └──▶ hardware reverts to factory
                     │
                     └── application configures
                              │
                              ├── SPI register interface decodes a valid transaction
                              │        └──▶ tickle the watchdog. Booted.
                              │
                              └── nothing tickles it before it expires
                                       └──▶ revert to factory
```

Three independent things have to go right, and each catches a different failure:

**The boot block's CRC** catches an interrupted update. The update flow writes the application image first, verifies it by readback, and writes the boot block *last*, so a power cut anywhere before that last write leaves a block that does not validate and a unit that stays in factory. Rolling back deliberately is erasing that one sector.

**The configuration CRC** is the FPGA's own, in hardware, and catches an application image that is corrupt in a way the boot block's CRC missed — flash decay after the fact, most obviously.

**The watchdog** catches the case neither CRC can see: an application image that configures perfectly and whose fabric is nonetheless dead. The image loads, the configuration CRC passes, and nothing works.

### What tickles the watchdog

The application image tickles the watchdog **only after its SPI register interface has decoded a first valid transaction**. Not on a timer, and not immediately on configuration: either of those would make the watchdog prove that the image loaded, which the configuration CRC already proved. Requiring a decoded transaction makes it prove that the fabric is *alive and talking*, which is the thing nothing else checks.

That works without a host because the FX3 firmware performs an identity read during its own initialisation. On a healthy device the tickle therefore happens within a second of power-up whether or not anything is plugged into the other end of the USB cable.

The watchdog period must sit comfortably above the worst case of FX3 boot plus that identity read. **That figure is a measurement, not an estimate, and it is taken on the bench before the policy is frozen into the factory image** — because it is frozen into the factory image, and a period set too short would mean a device that reverts to recovery whenever the FX3 boots slowly.

### Handover timing

Power-on now involves two configurations rather than one: factory, then application. The FX3 firmware currently assumes the FPGA is ready "well under a second" after power-up, and that assumption is documented in the firmware rather than enforced anywhere. It is re-measured against the double configuration before this design is committed to.

If the double configuration turns out to be slow enough to matter, the answer is the firmware's existing retry: it already probes for the register bank ten times at twenty-millisecond intervals and then keeps retrying every couple of seconds, precisely so that an FPGA reprogrammed while the FX3 keeps running is noticed. A slower handover extends the retry window rather than requiring a redesign.

## Provisioning a unit

Once, with a cable, and then never again:

1. Build both images and the combined provisioning `.jic`.
2. Program the `.jic` over the DE0-Nano's onboard USB-Blaster, exactly as the existing gateware procedure describes.
3. Power-cycle and confirm the unit boots to the application image — `IMAGE_ROLE` reads `0x01`.

From that point the application image is field-updatable over the single USB cable, and the cable comes out only for the cases the update mechanism cannot fix.

## Where the code is

| File | Holds |
| --- | --- |
| `fpga/factory/` | The frozen boot-loader image and its freeze policy (not yet written) |
| `fpga/application/` | The capture gateware (today `fpga/src/`) |
| `fpga/common/` | The register core, flash bridge and reconfiguration wrappers (not yet written) |
| `fx3/firmware/src/` | The EPCS driver that speaks through the bridge (not yet written) |

Related pages: [Device update mechanism](device-update-mechanism.md), [FPGA register interface](fpga-register-interface.md), [Update bundle format](update-bundle-format.md).
