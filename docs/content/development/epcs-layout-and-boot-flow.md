# EPCS layout and boot flow

How the DE0-Nano's configuration flash is divided, which half of it can be updated in the field, and what happens at power-on.

!!! note "Proved on hardware, 2026-08-15"

    A unit has been provisioned with a dual-image flash, updated over the USB cable, and seen to hand over from the factory image to the application image on a cold boot with nothing attached. The procedures are §6 of TESTING.md: **G0** provisioning, **G1** the update and the handover. The flash's silicon identifier (`0x16`) and the time an update takes are now measurements rather than expectations.

    Getting there cost five defects, every one of which had passed every host-side test and every simulation. Three of them are matters of policy rather than of code and are described on this page: [what the factory image must do before it hands over](#what-the-factory-image-does-before-it-hands-over), and [the bit orientation of the bytes in the flash](#the-bytes-in-the-flash-are-bit-reversed) — which is the one that survived all the others, because nothing in the update chain is capable of detecting it. TESTING.md §6 records all five.

    **One number is still written from documentation: the watchdog period**, which sits at the largest value the field holds. It and the double-configuration time the FX3's readiness assumption rests on must both be measured before the factory image is frozen, and the boot logic still owes a refusal to retry an image that configures and is dead. Those three are the remaining pre-freeze work, tracked in TESTING.md §8.

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
  factory/       the static boot-loader image. Own Quartus project, own README
                 stating the freeze policy. Expected to change ~never after its
                 first release.
  application/   the flashable capture gateware. Own Quartus project. Changes
                 freely; this is what the bundle carries.
  common/        Verilog shared by both: the spiRegisters core, the flash bridge,
                 the active serial and reconfiguration wrappers, version
                 generation, and sim/ models of the device primitives so the free
                 tools can simulate what the real parts do.
  provisioning/  the conversion that puts both images in one flash file.
```

Both images are compiled with remote update enabled — the Quartus assignment carries a Stratix III name on a Cyclone IV part, which is Quartus' own naming rather than a mistake — and the provisioning conversion marks exactly one of them as the factory page. Marking both is an error the converter refuses, which is a useful thing for it to refuse.

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

There are two encoders for this format and they are checked against each other. [`fpga/make-boot-block.py`](https://github.com/simoninns/DomesdayDuplicator/blob/main/fpga/make-boot-block.py) writes the block for a build, and its output is checked field by field, by offset, in `fpga/tests/test_boot_block.py`. `updateBootBlockEncode()` in the FX3 firmware writes it on the device at the end of every gateware update, and its output is checked against a golden block the Python encoder produced. The same twenty-four bytes appear again in the gateware testbench that reads them — so a change to the format in any one of the three fails in the other two.

**CRC32 rather than SHA-256, and this is the only place in the update chain where that is true.** The check runs in the factory image's fabric, where a SHA-256 core cannot be justified in an image whose entire purpose is to be small and never change. It defends against *corruption only* — and it only has to. Authenticity was settled before the boot block was ever written: the bundle's signature, the digest checked on the way into the device, and the digest recomputed from the flash after the write. An attacker who could write the boot block could write the application image too, and no digest in the factory image would help. The trade-off is stated here rather than left to be discovered, because a lone CRC in a document full of SHA-256 looks like an oversight until you know why it is not.

### The bytes in the flash are bit-reversed

The active serial configuration engine consumes each configuration byte **least significant bit first**, and SPI delivers bytes most significant bit first. The flash therefore holds every byte of an image bit-reversed with respect to the bitstream, and the reversal is not a convention anybody may choose: an image stored the other way round reads back perfectly and configures nothing.

What makes this a trap rather than a footnote is that the two ways of writing the flash put the reversal in different places:

* `quartus_pgm` programming a `.jic` over JTAG performs the reversal itself, on the way out. **The bytes in the `.jic` file are not the bytes that end up in the flash**, so comparing a file against a file proves nothing here.
* A device update writes the bundle's `.rpd` payload **verbatim**. Nothing between the file and the flash touches bit order — not the application, not the firmware, not the bridge, not the gateware.

So the orientation has to be right in the `.rpd` at the moment Quartus emits it, and it is: `fpga/application/DomesdayDuplicator.cof` sets `rpd_little_endian` to **0**, which is that converter's name for "emit the bytes the wire needs". The setting carries a comment saying so, because the option is named for byte endianness and what it decides here is bit order.

This is stated at length because **no check in the update chain can catch it**. The digest checked on the way in, the readback verify and the CRC-32 the boot block carries all compare the flash against what was sent, and a wire-backwards image matches itself exactly. The one component that does see the difference is the FPGA's own configuration CRC — and it has nowhere to report to. It fails the configuration, the device reverts to factory, the factory image validates the same image again and hands over again, and the unit reconfigures in a loop for as long as it is powered. Every check that can produce a message says the update succeeded.

It was found with a logic analyser on the flash's pins: the page that boots begins `56 EF EF …`, and the page the updater had written began `6A F7 F7 …` — the same header, every byte reversed.

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
            ├──▶ relock the flash bridge, releasing the configuration pins
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

What the watchdog does *not* yet catch is that same image being tried again immediately. The revert lands back in the factory image, which makes the same decision from the same flash and hands over again — a unit cycling at about three seconds a lap. `Cd_early` (below) narrows the window to exactly the images that configure and are dead, and the deliberate refusal to make a second attempt is still owed; it is one of the three things listed at the top of this page as due before the factory image is frozen.

### What the factory image does before it hands over

Four things in order, and the first bench session found that three of them had been wrong in a way only hardware could show.

**Relock the flash bridge.** Reading the boot block means the fabric is driving the flash's chip select, clock and data lines — which are the very pins the configuration engine is about to drive. So the bridge is relocked, releasing them, on *both* paths out of the boot decision and before anything is armed. The firmware has always had this discipline for its own use of the bridge; the gateware acquired it after a fabric that was still holding the pins met an engine trying to read through them.

**Write the two option bits the handbook requires of a factory configuration.** `Osc_int` runs the reconfiguration logic and the watchdog from the device's internal oscillator, which is the one clock guaranteed to survive a configuration; `Cd_early` has the engine check the candidate image early rather than committing to it blind. This design wrote neither, and the omission is silent — the block accepts the rest of the setup and behaves subtly differently.

**Stage the boot address.** It is written as a full 24-bit *byte* address; the block stores bits 23:2 and appends two zero bits when it boots. The value is 32-bit aligned either way, so the pre-shifted and unshifted encodings are one shift apart and both look plausible in a read-back — which is what made this the slowest of the five defects to settle. It is now confirmed twice on hardware: read back from the block's input register, and seen on the analyser as a FAST_READ issued at exactly `0x200000`.

**Hold every strobe long enough.** The reconfiguration request and the watchdog tickle are specified with a 250 ns minimum and this design drove them for 200 ns. They are 800 ns now; the block runs at a quarter of the system clock and a generous strobe costs nothing.

The first three are pinned by `fpga/tests/tb_bootLoader.v`, which fails if the bridge is still driving the flash pins at handover, if either option bit is clear, or if the staged address is not the application's. The fourth is a constant with its requirement written beside it.

The block's own account of all of this is readable at run time through registers `0x30`–`0x37` — the mode it is in, the previous attempt's trigger condition, and every field it has been given — which is how these defects were measured rather than guessed. They are described on the [FPGA register interface](fpga-register-interface.md) page.

### The clock the reconfiguration block runs at

Not the system clock. The block is specified on this device for a 25 ns minimum period and 10.1 ns minimum high and low times, which Quartus checks and which the 80 MHz system clock violates by a factor of two — so it is given a divide-by-four of it, at 20 MHz, and the constraint that says so is in both images' `.SDC`.

Because the block runs slower than the logic driving it, every signal into it is a level rather than a pulse: a parameter write is held until the block acknowledges it by raising busy, and reconfiguration and tickle requests are stretched over enough system clocks that the block cannot miss one between its own edges.

### What tickles the watchdog

The application image tickles the watchdog **only after its SPI register interface has decoded a first valid transaction**. Not on a timer, and not immediately on configuration: either of those would make the watchdog prove that the image loaded, which the configuration CRC already proved. Requiring a decoded transaction makes it prove that the fabric is *alive and talking*, which is the thing nothing else checks.

That works without a host because the FX3 firmware performs an identity read during its own initialisation. On a healthy device the tickle therefore happens within a second of power-up whether or not anything is plugged into the other end of the USB cable.

That the mechanism works is settled: with the watchdog enabled at its ~54 second period, a bench unit ran well past the timeout without reverting, which it could only do if the FX3's register traffic were resetting the timer.

What is not settled is the *period*. It must sit comfortably above the worst case of FX3 boot plus that identity read, and it is still at the largest value the field holds rather than at a measured margin. **That figure is a measurement, not an estimate, and it is taken before the policy is frozen into the factory image** — because it is frozen into the factory image, and a period set too short means a device that reverts to recovery whenever the FX3 boots slowly.

### Handover timing

Power-on now involves two configurations rather than one: factory, then application. The factory image's own half is quick — validating a compressed application image over the bridge takes about a quarter of a second — but the two configurations themselves have not been timed.

The FX3 firmware assumes the FPGA is ready "well under a second" after power-up, and that assumption is documented in the firmware rather than enforced anywhere. Measuring the double configuration against it is verification item V5, and it is outstanding: the bench sessions so far have shown the handover working, not how long it takes.

If the double configuration turns out to be slow enough to matter, the answer is the firmware's existing retry: it already probes for the register bank ten times at twenty-millisecond intervals and then keeps retrying every couple of seconds, precisely so that an FPGA reprogrammed while the FX3 keeps running is noticed. A slower handover extends the retry window rather than requiring a redesign.

## Provisioning a unit

Once, with a cable, and then never again:

1. Build both images and the combined provisioning `.jic` — one command, `./fpga/build-local.sh`, which also writes the boot block for the application image it just built.
2. Check `provisioning/DomesdayDuplicatorProvisioning.map` before programming anything. It must place the factory image at `0x000000` and the application image at `0x200000`. That file is the only check that the converter put the images where this page says they are, and everything downstream is meaningless if it did not.
3. Program the `.jic` over the DE0-Nano's onboard USB-Blaster, exactly as the existing gateware procedure describes. It takes about eleven seconds, and the programmer prints the flash's silicon identifier as it goes — `0x16` for the EPCS64, which must match what the firmware expects.
4. **Power-cycle, and treat this as mandatory rather than as a convenience.** The programmer leaves the FPGA running its own serial flash loader rather than this project's gateware, so a unit that has just been programmed answers nothing on the register link. An update attempted before the power cycle is refused with *"the FPGA is not answering"* — which is the gate working, not a fault.
5. The unit comes up in the **factory image** — `IMAGE_ROLE` reads `0x00` and the application reports a unit in recovery — *provided the boot block sector was erased*. The JTAG erase is page-selective, so a unit being reprovisioned over a working installation keeps its old boot block, and if the flash still matches that block's CRC it boots straight into the application image instead. Both outcomes are correct; which one you get depends on what was there before.
6. Write the boot block, which is the last step of any gateware update and is what makes an application image count.
7. Power-cycle again and confirm `IMAGE_ROLE` now reads `0x01`.

Step 6 is the one that needs the update path, and the update path is what writes it: the application's **Reinstall gateware** button performs the whole of an ordinary gateware update, and its last act is the boot block. So **a freshly provisioned unit is in recovery until a gateware update has been performed on it.** That is not a workaround; it is the same commit ordering every later update follows, exercised once at the beginning. What it does mean is that provisioning and the first gateware update are one procedure rather than two — and that the first thing a newly provisioned unit proves is the mechanism every later update depends on.

The boot block is not in the `.jic` because Quartus' converter cannot place arbitrary data in one for this device family. That was tried, with every spelling of the option its own converter recognises, and it refuses the conversion; the `boot-block.bin` written beside the `.jic` is the same twenty-four bytes, for the same image, ready for whatever writes it.

From that point the application image is field-updatable over the single USB cable, and the cable comes out only for the cases the update mechanism cannot fix.

## Where the code is

| File | Holds |
| --- | --- |
| `fpga/factory/bootLoader.v` | The boot decision: read the block, check it, check the image it names, hand over or stay |
| `fpga/factory/crc32.v` | The checksum, checked against the published CRC-32 check value |
| `fpga/factory/README.md` | The freeze policy, and what is still to be confirmed on a bench |
| `fpga/common/flashBridge.v` | The bridge both images carry, and its lock |
| `fpga/common/remoteUpdate.v` | The reconfiguration trigger, the watchdog, the option bits, and the block's read-back diagnostics |
| `fpga/application/DomesdayDuplicator.cof` | The conversion that emits the `.rpd` — and the bit orientation it emits it in |
| `fpga/common/sim/` | Models of the two device primitives and of the EPCS64, so the boot path simulates against a flash rather than a stub |
| `fpga/make-boot-block.py` | The encoder for the format above |
| `fpga/provisioning/` | The conversion that puts both images in one flash file |
| `fx3/firmware/src/epcs-flash.h` | The EPCS driver that speaks through the bridge |
| `fx3/firmware/src/update-protocol.h` | The device's copy of the layout above, and its encoder for the boot block |

Related pages: [Device update mechanism](device-update-mechanism.md), [FPGA register interface](fpga-register-interface.md), [Update bundle format](update-bundle-format.md).
