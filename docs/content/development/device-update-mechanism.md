# Device update mechanism

How a Domesday Duplicator updates its own firmware and gateware over the one USB cable it already has, with no jumper, no second cable, no Quartus and no command line.

!!! note "Specification first, and now built end to end"

    This page is **normative**: it defines the protocol, the register map and the compatibility rules that the firmware, the gateware and the capture application are built to. All of it now describes something that exists.

    **Target 0 — the FX3 boot EEPROM — is implemented and proved on hardware.** The firmware answers `0xD0`–`0xD4`, brings up its own I2C block, writes and verifies its own boot EEPROM, and advertises a protocol version in `bcdDevice`. The application installs a bundle through it and `ddd-update` does the same headlessly.

    **Target 1 — the FPGA's EPCS — is implemented and not yet proved on hardware.** The firmware drives the EPCS through the gateware's flash bridge: it identifies the flash, erases and programs the application region, reads it back to check the digest, writes the boot block last, and answers `0xD5`. Both gateware images implement register map version 2 around it. What has not happened is the bench — no unit has been provisioned with a dual-image flash, so the whole of the gateware path has been exercised only against the host-side tests and the free-tool simulation.

    Two numbers on this page are therefore still written from documentation rather than from a measurement: how long a gateware update takes, and the flash's silicon identifier. Both are in the [EPCS layout and boot flow](epcs-layout-and-boot-flow.md) page's list of what the first bench session settles.

## The problem

Updating a Duplicator today needs tools and physical intervention. The FX3 firmware needs `fx3-programmer`, the J4 boot jumper and a power cycle; the FPGA gateware needs Quartus, the DE0-Nano's second USB cable and `quartus_pgm`. Both live in the *hardware programming* section of this site and both are developer procedures. A user with an assembled, cased unit cannot follow either.

The two halves of that problem have different shapes, and only one of them is a software problem.

The FX3 boots from an I2C EEPROM on the Explorer Kit. Its I2C pins are dedicated — not shared with the 16-bit GPIF bus or the UART — so the running firmware *can* bring up the I2C block and rewrite its own boot EEPROM. It simply does not today.

The FPGA is harder, and the reason is in the wiring rather than in the code: **the FX3 has no electrical path to the FPGA's configuration circuitry**. A trace of the schematics finds the GPIF data bus, `USB_PCLK`, the CTL lines carrying the SPI register link and `nReset`, sixteen wired-but-unused data lines and one spare — and no JTAG, no active-serial pins, no `nCONFIG`, no `nSTATUS`, no `CONF_DONE`, no MSEL. The EPCS64 configuration flash sits on the DE0-Nano's own dedicated pins and never leaves that board. So the only route from the host to the EPCS is *through the FPGA fabric*, using the Cyclone IV's `asmiblock` primitive to reach the flash and `rublock` to trigger reconfiguration.

That single fact shapes the whole design. It is also why loading the gateware from the host at every connection — the obvious way to make staleness impossible — is not merely inadvisable here but electrically impossible.

## One agent, two targets

```
ddd-gui ──EP0 vendor requests──▶ FX3 application firmware
                                    │
                                    ├─ target 0:  I2C block ──▶ boot EEPROM (M24M02)
                                    │
                                    └─ target 1:  bit-banged SPI ──▶ spiRegisters
                                                        │ (flash bridge unlocked)
                                                        └──▶ asmiblock ──▶ EPCS64
                                                             rublock  ──▶ reconfigure
```

The FX3 application firmware is the single on-device update agent. It is its own flasher for its own EEPROM, and it is the host's proxy for the EPCS. The gateware stays deliberately dumb: it offers a byte-at-a-time SPI pass-through and a reconfiguration trigger, and every decision about erase order, page programming and verification lives in C, where it can be reviewed and — for the pure parts — tested without hardware.

The alternative was a flash command engine in the gateware, with the firmware relaying. It is functionally equivalent and was rejected because it puts the complexity in the half of the project with the weakest test coverage. The gateware's job here is small enough to review line by line, which matters more than usual because half of it ends up in an image that can never be updated in the field.

## The vendor protocol

Six requests, all on endpoint 0. Existing codes are untouched: `0xA0` belongs to the Cypress boot ROM, `0xB0`/`0xBA`/`0xBB` to the Cypress flash programmer personality, and `0xB5`–`0xB8` to this firmware's capture and register interface.

`wIndex` selects the target throughout: **0** is the FX3 boot EEPROM, **1** is the FPGA EPCS application image. A request naming any other target stalls.

| Request | `bmRequestType` | Direction | Data stage | Purpose |
| --- | --- | --- | --- | --- |
| `0xD0` `UPDATE_STATUS` | `0xC0` | IN | 16 bytes | Phase, byte counter, last error, capability bits |
| `0xD1` `UPDATE_BEGIN` | `0x40` | OUT | 40 bytes | Payload length and SHA-256; enters update mode |
| `0xD2` `UPDATE_DATA` | `0x40` | OUT | ≤ 2 KiB | One chunk; `wValue` is the chunk index |
| `0xD3` `UPDATE_FINISH` | `0x40` | OUT | none | Finish writing, verify by readback, commit |
| `0xD4` `DEVICE_RESET` | `0x40` | OUT | none | Cold reset; the device re-enumerates |
| `0xD5` `FPGA_RECONFIG` | `0x40` | OUT | none | Trigger reconfiguration from the application image |

### `0xD1` UPDATE_BEGIN

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Payload length in bytes, little-endian |
| 4 | 32 | SHA-256 of the payload |
| 36 | 4 | Flags, little-endian; all bits reserved and zero |

The digest arrives *before* the first byte of payload, which is what lets the firmware hash the incoming stream as it arrives and abort before anything is committed. It is SHA-256 and not a CRC because it is the same number the bundle's manifest carries and the same number CI computed at build time — one digest, checked at every hand-off, is the rule the whole chain is built on.

`UPDATE_BEGIN` is refused while a capture is running, and a capture is refused while an update is in progress. The two are mutually exclusive by state, not by convention.

### `0xD2` UPDATE_DATA

`wValue` carries the chunk index, starting at zero and incrementing by one. A chunk that arrives out of order fails the transfer rather than being buffered: the host is a program, not a network, and a gap in the sequence means something has gone wrong that reordering would hide.

Every chunk but the last is the full chunk size the device advertises in `0xD0`, and **every chunk but the last must be a whole number of the target medium's pages** — 64 bytes for the EEPROM, 256 for the EPCS. That is the one constraint the protocol puts on the host, and it is there so that the firmware can write a chunk straight to the medium with no assembly buffer in between: a page write that runs past the end of its page wraps to the start of the same page on both media, so the alignment has to hold somewhere and the host is the cheapest place for it to hold.

A host that takes the advertised chunk size and rounds it *down* to a multiple of **256** satisfies both targets for any advertised size, which is what the application does rather than assuming 2048. One alignment for both, because a whole number of the larger page is a whole number of the smaller one.

The last chunk carries whatever is left. On the EEPROM the firmware zero-pads its final page on the way out, and the padding is outside the payload the digest covers; on the EPCS nothing beyond the payload is written at all, because an erased flash byte already reads as what an unwritten byte should be.

Chunks are acknowledged by the control transfer itself; per-target flow control — I2C page-write timing, EPCS busy polling — happens inside the firmware between chunks, so the host never has to know the medium's timing. **The host must therefore put no deadline on `0xD2`**, and this matters far more for the gateware than for the firmware: the chunk that opens a new 64 KiB flash sector pays for that sector's erase, which the part specifies at a second typically and three at worst. Roughly one chunk in thirty is seconds long, and the application's progress line says so rather than leaving a bar to pause unexplained.

### `0xD3` UPDATE_FINISH

The firmware completes any outstanding writes, reads the written region **back from the medium**, recomputes SHA-256 over what it read, and compares it against the digest from `UPDATE_BEGIN`. Only on a match does it write the commit record. The result is read with `0xD0`; `UPDATE_FINISH` itself does not stall on a verification failure, because "the update failed and here is why" is more useful than a stalled endpoint.

`UPDATE_FINISH` **returns immediately**. It checks the stream digest — the cheap half, over bytes already in RAM — and if that passes it moves to the verifying phase and hands the readback to the firmware's application thread. The readback is tens of seconds of I2C for the EEPROM and minutes for the EPCS, and a control request that took that long to answer would be abandoned by the host long before it did. The host watches the `bytes verified` counter and waits for the phase to reach complete or failed.

That split is also why the phase, rather than a lock, decides which of the firmware's two threads may touch the medium. Every request the USB setup callback would honour is refused during the verifying phase, so the application thread has the medium to itself; and `UPDATE_STATUS`, which is what the host's progress display is made of, stays answerable throughout because it takes no lock at all.

The same asymmetry explains which failures stall and which do not. A request whose *shape* is wrong — an `UPDATE_BEGIN` that is not 40 bytes, a chunk larger than the advertised maximum — is refused before its data stage is read, and stalls. A request whose content is refused has already had its data read, and the USB hardware acknowledges a control-OUT transfer as soon as its last byte arrives; so those are answered through `UPDATE_STATUS`, which is where a host should be looking anyway.

### `0xD0` UPDATE_STATUS

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | Phase: idle, receiving, writing, verifying, complete, failed |
| 1 | 1 | Last error code, zero when there has been none |
| 2 | 2 | Maximum chunk size the device accepts, little-endian |
| 4 | 4 | Bytes received so far |
| 8 | 4 | Bytes written to the medium so far |
| 12 | 4 | Bytes verified so far |

Three separate counters, because they move at very different speeds and a progress bar driven by one of them would lie during the other two. Transferring a gateware image over EP0 takes seconds; erasing and programming the EPCS takes minutes; verifying takes about as long as reading it back. A user watching a single bar would see it fill quickly and then apparently stop.

The status request is answerable at any time, including when no update is in progress, and it is how the host discovers the chunk size rather than assuming one.

### `0xD4` DEVICE_RESET and `0xD5` FPGA_RECONFIG

`0xD4` is `CyU3PDeviceReset(CyFalse)` — a cold reset, so the FX3 re-reads its boot source and comes back running whatever is now in the EEPROM. It also closes a long-standing gap: until now the host had no way to reboot the device at all.

`0xD5` writes the reconfiguration trigger through the gateware's `rublock` control. Reconfiguration stops `USB_PCLK` underneath the GPIF, so `0xD5` is always followed by `0xD4`: the FX3 is reset rather than left holding a capture path whose clock has gone away.

It is refused — by stalling — when no gateware carrying the flash bridge is answering, and while a transfer is open. Acknowledging either would tell a host that a reconfiguration had happened when nothing had, and the second case would pull the bridge out from under a write in progress.

What the FPGA reconfigures *to* is the factory image, not the application image. The factory image then makes the same boot decision it makes at every power-on, with whatever the flash now holds. That asymmetry is deliberate: an update ends with the device booting the way it always boots, rather than the way a special case in the application image thought it should, so the path taken after an update is the path every unit has exercised since it was provisioned.

## Writing the EPCS: what target 1 actually does

The route is four links deep — bit-banged SPI to the register bank, the flash bridge at `0x20`–`0x22`, the `asmiblock` primitive, then the flash — and every decision along it is made in the firmware. The gateware shifts bytes and knows none of the commands below.

**At `0xD1`**, before a byte is accepted: the firmware checks that a gateware carrying the bridge is answering, unlocks the bridge, reads the flash's silicon identifier with `0xAB`, and checks that the device it names is large enough to hold the image at the application address. A flash that answers `0x00` or `0xFF` has not answered at all — those are the two readings of a line with nothing driving it — and both are refused with `UPDATE_ERROR_HARDWARE`. Nothing is erased at this point, so an update abandoned before its first chunk leaves the previous gateware intact and running.

**At each `0xD2`**, in address order from `0x200000`: where a chunk crosses into a sector that has not been erased, that sector is erased; then each page of the chunk is programmed with `WREN` and `0x02`, with the status register polled until the write-in-progress bit clears. The image is written strictly forwards, so erasing the sector an address opens is both necessary and sufficient, and an image occupying six sectors costs six erases rather than erasing a device that is mostly factory image.

**At `0xD3`**, and then on the application thread: the stream digest is checked, and the whole written region is read back off the flash and hashed. One pass, two accumulators — SHA-256 for integrity link 6, and the CRC-32 the boot block will carry, so the checksum the factory image validates at every power-on is computed from the flash rather than from what the host sent.

**Only then the boot block**, at `0x100000`: its sector erased, the twenty-four bytes programmed, and read back and compared before the update is called complete. That last write is the commit, and everything above it is arranged so that losing power at any point before it leaves a unit that boots the factory image and says so.

The bridge is unlocked for the duration of one flash operation and locked again afterwards, which also releases the flash's pins — so between operations the gateware is not connected to the flash at all. Locking mid-operation would release those pins with a command in flight, which is why the unlock spans the whole of one rather than each framed command inside it.

**Throughput is the open question, not correctness.** A page program is one framed register transaction of 260 bytes, because `BRIDGE_DATA` does not auto-increment — so writing costs one link-crossing per flash byte. Reading costs four, because latching the byte that came back needs a write to shift it and a read to collect it, and the register bank drives zeros on `MISO` during a write transaction so the two cannot share a frame. Write once, read back once, and a 350 KB image is a minutes-long operation. Whether that is two minutes or five is a measurement (verification item V6 in TESTING.md), and if it proves to dominate, the fix is a bridge that starts the next shift on a read — a change to the gateware, and therefore to the frozen half of it, which is exactly why it is not being made ahead of the measurement.

### Commit ordering is the safety mechanism

Neither target is made valid until it has been verified, and in both cases the *last* write is the one that makes the image count.

For the **FX3**, the first EEPROM page carries the `'CY'` signature the boot ROM looks for, and the start of the section table. That page is held back: the rest of the image is written and verified first, and the first page is written last. An update interrupted anywhere in the middle leaves an image the boot ROM rejects, and the kit falls back to the USB bootloader — a personality the application recognises and can repair from.

Which of the boot ROM's two checks does the rejecting depends on what was there before, and it is worth being exact about, because the two cases sound the same and are not:

- on a **blank or never-programmed** EEPROM there is no `'CY'` at offset zero, so the signature check refuses it outright;
- on a **device being re-flashed** — the ordinary case — the *previous* image's first page is still in place and still carries a valid `'CY'`. The signature check passes. What refuses it is the boot ROM's **image checksum**, computed over section data that is now the new image's while the section table describing it is the old one's. The two cannot agree.

So the held-back page buys the guarantee on a fresh device and the checksum buys it on a re-flash. Both are link 7 of the integrity chain, which names both checks for this reason. Verified on the bench: an update interrupted mid-transfer on a programmed unit brings it back as `04b4:00f3`.

A stricter ordering is available if that ever proves too subtle to rely on — invalidate the first page *before* the first body byte is written, so the signature check alone decides and the checksum is never load-bearing. It is one extra page write and it costs one thing: a transfer that fails before any body write would then still leave the device needing a repair, where today it is untouched. That trade has not been made, and this note is here so that it is a decision rather than an oversight.

For the **FPGA**, the application image is written and verified first, and the boot block that points at it is written last. An interrupted gateware update leaves an invalid boot block, and the unit simply stays in the factory image. Rolling back is erasing one sector. The layout and the boot decision are on the [EPCS layout and boot flow](epcs-layout-and-boot-flow.md) page.

In both cases the failure mode of an interrupted update is a device in a known rescue state, not a device that half-works.

## Register map version 2

The gateware's register interface gains a role register and a flash bridge. Everything in [version 1](fpga-register-interface.md) is unchanged, and the identity block at `0x00`–`0x0A` is frozen across all map versions, so a host that does not recognise the map version can still read who it is talking to.

| Address | Name | Access | Reset | Purpose |
| --- | --- | --- | --- | --- |
| `0x0B` | `IMAGE_ROLE` | RO | — | `0x00` factory image, `0x01` application image |
| `0x20` | `BRIDGE_UNLOCK` | RW | `0x00` | Unlock sequence; the bridge is inert until it is written |
| `0x21` | `BRIDGE_CONTROL` | RW | `0x00` | Chip select assert and deassert |
| `0x22` | `BRIDGE_DATA` | RW | — | One SPI byte out, the simultaneously shifted byte in |
| `0x23` | `RECONFIG_CONTROL` | RW | `0x00` | Arm and trigger reconfiguration; watchdog tickle |

`MAP_VERSION` at `0x01` reads `0x02` for gateware implementing this.

**`IMAGE_ROLE` exists so that "which image am I running?" is a question with an answer.** Without it the only way to tell a factory image from an application image would be to infer it from what else is present, and a recovery state the application has to guess at is a recovery state it will sometimes get wrong.

**`BRIDGE_DATA` does not auto-increment**, unlike every other register in the map. It is a port rather than a location: each write shifts a byte out to the EPCS and latches the byte that came back, and a read returns that latched byte. A multi-byte SPI transaction is therefore a run of writes and reads to one address, which is exactly what the address auto-increment would otherwise break.

**`BRIDGE_UNLOCK` is not a formality.** Until it has been written with its magic sequence the bridge is inert and `BRIDGE_CONTROL` and `BRIDGE_DATA` do nothing at all. The registers reachable over this link are reachable by anything that can send `0xB8`, and the EPCS holds the only copy of the gateware; a stray write must not be able to reach the flash. The unlock is cleared by `nReset` and by a completed reconfiguration.

## Machine-readable versions, and what they gate

Compatibility is decided machine-to-machine, in both directions, and never inferred from commit strings. A commit identifies a build exactly and orders nothing: `a1b2c3d4` is neither newer nor older than `e5f6a7b8`, and any code that appears to compare them is comparing text.

Two integers carry the compatibility information, one from each half of the device:

- the gateware's **register-map version**, register `0x01`, which already exists;
- the firmware's **protocol version**, carried in the USB descriptor's `bcdDevice` field. That field was a dead `0x0000` until this work, and it is the ideal place for it: the host reads it during enumeration, before opening the device and without sending a single vendor request, so a device speaking a protocol this application does not understand can be recognised before anything is asked of it.

    The version is the **high byte** and the low byte is zero, so version 1 is `0x0100` and `lsusb` reads it as `1.00` rather than as something that looks like a mistake. The host compares the high byte. Firmware predating the field reports zero, which is not a version and must not be ordered against one — the application treats it as old firmware it may update and must not make claims about.

Both follow the same bump rule. **An additive change does not bump the version; a change that would break an existing host does.** Adding a register, a status field or a new request number is additive — an old host ignores what it does not know about. Changing the meaning of an existing field, removing one, or changing the order of a sequence is breaking.

The application is built knowing the **range** of each version it supports, not a single expected value. A build that only accepted the exact version it shipped alongside would treat every additive change as an incompatibility, which is the same as having no versioning at all.

### The install-time gate

Before any byte is streamed to the device, the application checks the bundle's manifest against itself:

- the manifest's schema version must be one this build knows. A manifest from the future may mean something different by a field of the same name, and reading the fields it recognises and ignoring the rest is how a device gets flashed with something nobody described;
- the manifest's `minimum_application_version` must be no newer than this build. If it is, the install button is **disabled**, with "update the application first" — a user cannot use this application to flash the device past this application's own understanding;
- each component's declared interface version must fall in the range this build supports.

The ordering users must follow — application first, then device — is therefore the only ordering the interface permits, rather than something the release notes ask for.

### The connect-time gate

The second-order case is a device that meets an old application having been updated elsewhere by a newer one. On every connect, the application compares what the device reports against its supported ranges:

- device **newer** than this build understands: a clear "this firmware requires a newer application" state, capture disabled, and the application-update route offered. A wrong-protocol capture must not limp along — the failure mode of this device is a file that looks fine and is subtly wrong forever;
- device **older**: the existing mismatch warning, now with the device-update offer attached.

The commit-prefix comparison the firmware dialog already does stays exactly as it is, as a freshness hint. It no longer carries any compatibility weight, because it never could.

### Downgrades

Permitted, deliberately. Rollback is a feature and the archive of release bundles on GitHub is the rollback source. A downgrade passes through the same two gates as an upgrade, so a bundle whose `minimum_application_version` is newer than the running application is refused whichever direction it is going.

## The integrity chain

The rule: **one digest, SHA-256, computed once at build time and checked at every hand-off, all the way to the flash readback.** No link trusts the previous link's verification. Authenticity — nothing *replaced* — comes from the pinned-key signature on the manifest; integrity — nothing *corrupted* — comes from re-checking the same digest at each link.

| # | Hand-off | Threat | Check |
| --- | --- | --- | --- |
| 1 | Source → binaries | Tampered or irreproducible build | Every payload is built hermetically by CI from the tagged commit; no maintainer-built binary enters the chain; SHA-256 digests land in the CI-produced provenance file published with the release |
| 2 | Build environment → binaries over time | Environment drift, silent non-reproducibility | The bitstreams are byte-identical across rebuilds; a scheduled audit rebuilds the latest release tag and compares digests against the published provenance |
| 3 | Bundle assembly | Wrong payload bundled | The assembler computes each payload's SHA-256 into `manifest.json`, re-reads the finished archive to check both, and signs the manifest |
| 4 | GitHub → application, or hand-download → file picker | Replaced asset, corrupted download | TLS in transit; then, identically for both paths, signature verification of `manifest.json` against the public key compiled into the application, then per-file SHA-256 of every payload. No install button until both pass |
| 5 | Application → FX3 over EP0 | Corruption in transit or in host memory | `UPDATE_BEGIN` carries the length and SHA-256; the firmware hashes the chunk stream as it arrives and aborts before commit on a mismatch |
| 6 | FX3 → EEPROM or EPCS | Write errors, power loss, flash defects | Full readback of the written region; SHA-256 recomputed **from the medium** and compared against the `UPDATE_BEGIN` digest; only on a match is the commit record written |
| 7 | Every subsequent boot | Flash corruption in the field | FX3: boot-ROM image checksum and `'CY'` signature, falling back to the USB bootloader. FPGA: configuration CRC falling back to factory, plus the factory boot logic checking the boot block's **CRC32** |
| 8 | After the update | The wrong image actually running | Commit identities read back from the live device — product string, gateware registers — and compared against the manifest's expected identities |

Link 7's CRC32 is the one deliberate exception to the single-digest rule, and it is worth stating why rather than leaving it to be discovered. It runs in the factory image's fabric, where a SHA-256 core cannot be justified in an image whose whole purpose is to be small and frozen. It defends against *corruption only* — and it only has to, because authenticity was settled at links 4 to 6, before the boot block was ever written. An attacker who could write the boot block could write the application image too, and no digest in the factory image would help.

## Rescue states

Both targets fail into a state the application can recognise and repair, and neither is a state a user has to diagnose.

| What happened | What the device does | What the application shows |
| --- | --- | --- |
| Firmware update interrupted | Boot ROM rejects the image, the kit enumerates as the Cypress bootloader `04b4:00f3` | "Recovery mode", with a one-click **Program this device** — the same button, and the same procedure, that brings up a board which has never been programmed |
| Gateware update interrupted | Boot block invalid, the FPGA stays in the factory image | "Recovery gateware running — reinstall gateware", with a one-click repair |
| Application image wedged | Remote-update watchdog expires, the FPGA reverts to factory | As above |
| Both, or something stranger | | The bench procedures in the provisioning appendix, which need a cable |

The application's own words for these states and this page's words are meant to be the same words. A user reading "recovery gateware running" in a dialog and then finding a page that calls it something else has been given two problems.

## Personalities, and reaching a device that has no firmware

The FX3 has no flash of its own. It boots from an I2C EEPROM, and if the boot ROM does not accept what it finds there it stops and waits for a host instead — so a Duplicator wears one of three identities on the bus, and which one it is decides what may be asked of it.

| Identity | What is running | What answers |
| --- | --- | --- |
| `1209:2347` | The Duplicator's firmware | Capture, the register requests, `0xD0`–`0xD5` |
| `04b4:00f3` | The FX3 boot ROM | `0xA0`, RAM download, and nothing else |
| `04b4:4720` | The Cypress secondary loader, if one was left running | Its own I2C commands; confirmed by the `0xB0` probe answering `FX3PROG` |

The match is on exact identifier pairs and never on the Cypress vendor identifier alone: the SuperSpeed Explorer Kit's on-board USB-UART is `04b4:0007` and is powered whenever the board is, so a wildcard would list the debug serial port as a Duplicator in recovery. `DeviceInfo::personality` carries the answer, it is decided during enumeration from the descriptor alone, and `SelectDevice` takes a `DeviceSelection` that defaults to the narrow set — a caller that has not thought about recovery devices cannot be handed one to capture with, and the two callers that have thought about it (the firmware dialog and `ddd-update`) say so at the call site.

### Three ways in, one way out

A device is at `04b4:00f3` for one of three reasons, and **they are indistinguishable on the wire**:

- the kit has never been programmed, which is how every SuperSpeed Explorer Kit arrives;
- an update was interrupted before its last write, which is the state the held-back first page is designed to produce;
- the PMODE jumper is fitted, which is a developer doing it on purpose.

All three are repaired the same way, so the application does not try to tell them apart. It offers *"program this device"* rather than *"repair"*, and says in one sentence that the device has either never been programmed or had an update interrupted — because somebody holding a board they have just soldered has not broken anything, and a guess between the two cases would be wrong half the time.

### The route: the device programs itself

The boot ROM implements one command — put these bytes at this address — and one way of ending, a download with no data stage, which is the jump to the entry point. That is enough:

1. the host parses the update bundle's own `firmware.img`, which is a container rather than a flat binary: a four-byte header, a run of sections each with a load address, a termination record carrying the entry point, and a checksum over the section payloads (AN76405 §4.4, and the table in `fx3/mkimage/README.md`);
2. each section is downloaded with `0xA0`, in transfers of at most 2 KiB;
3. the entry point is jumped to, and the device re-enumerates as `1209:2347` — running its proper firmware, out of RAM;
4. from there it is an **ordinary update**. The firmware that has just been loaded writes the EEPROM through `0xD1`–`0xD3`, hashing the incoming stream and then the readback, exactly as a routine update does.

The consequence worth stating plainly: **a first-time programming of a bare board is covered by the whole integrity chain**, links 4 through 8, rather than by a shortcut around it. Nothing about the recovery path writes the EEPROM by any other route, and the code that does the writing is the code every other test in the suite exercises.

The Cypress secondary loader is deliberately *not* used, although `fx3-programmer` uses it and the route was available. Using it would mean vendoring a second copy of an LGPL binary blob inside the application and shipping it in every package, writing a second EEPROM-paging implementation outside the digest chain, and verifying by byte comparison rather than by SHA-256. This project's own firmware is a better programmer for this project's own EEPROM.

### The path a device comes back at

`IDeviceProgrammer::WaitForApplication` returns the path rather than assuming it, because the path does not survive the change of identity on every platform. libusb paths are built from bus and port numbers, which are the same before and after; Windows paths are device interface paths, which carry the product identifier and therefore change. The programmer records which Duplicators were already working before it starts, so on Windows the device that came back is the application-personality device that was not in that set — which is also what stops a second, healthy Duplicator on the same machine being mistaken for the one being recovered.

### Windows needs the driver bound

Windows binds drivers by USB identifier, and a device in recovery mode reports different identifiers from a working one. The driver bound to a working Duplicator is therefore not bound to the same unit in recovery, and until WinUSB has been bound to `04b4:00f3` the application cannot open it at all — the device will not appear. This is a once-per-machine step with Zadig, and it is documented for users on the [If an update fails](../capture-application/if-an-update-fails.md) page. Linux needs nothing: `fx3/programmer/configs/70-domesday-duplicator.rules` already covers the Cypress identifiers with a wildcard. macOS binds nothing.

## Where the code is

| File | Holds |
| --- | --- |
| `ddd-gui/src/capture/update_bundle.h` | The bundle reader, and the order the checks happen in |
| `ddd-gui/src/capture/update_manifest.h` | The manifest model and the version comparison |
| `ddd-gui/src/capture/update_key.h` | Which signatures a build accepts, and what each one proves |
| `ddd-gui/src/capture/update_gate.h` | The install-time compatibility gate |
| `ddd-gui/src/capture/device_updater.h` | The seam every update runs through, and the status packet |
| `ddd-gui/src/capture/device_programmer.h` | The second seam: a device in its boot ROM, and the three things it can be asked |
| `ddd-gui/src/capture/boot_image.h` | The FX3 boot image format, read from the host's side |
| `ddd-gui/src/capture/device_recovery.h` | The prelude that turns a recovery device into one the orchestrator can drive |
| `ddd-gui/src/capture/usb_device_info.h` | Personalities, and which of them a selection will consider |
| `ddd-gui/src/capture/update_orchestrator.h` | The flow: verify, program, reset, confirm |
| `ddd-gui/src/capture/update_cli.h` | `ddd-update`, over the identical engine path |
| `ddd-gui/src/capture/digest.h` | SHA-256, the one digest |
| `ddd-gui/src/capture/minisign_verify.h` | Signature verification |
| `ddd-gui/src/capture/wire_protocol.h` | The host's copy of the request numbers and register addresses |
| `ddd-gui/src/gui/update_page.h` | The staged flow a user sees |
| `tools/make-update-bundle.sh` | Bundle assembly and signing |
| `fx3/firmware/src/update-protocol.h` | The protocol's decisions, host-testable and SDK-free: both media's paging arithmetic, the boot block's format and the CRC-32 it carries |
| `fx3/firmware/src/update-agent.h` | The on-device flasher: both targets, the readbacks, and the commit ordering |
| `fx3/firmware/src/epcs-flash.h` | The route to the configuration flash, and the only place the EPCS command sequences live |
| `fpga/common/flashBridge.v` | The flash bridge, and the lock that keeps it inert |
| `fpga/common/remoteUpdate.v` | The reconfiguration trigger and the configuration watchdog |
| `fpga/factory/bootLoader.v` | The boot decision the factory image makes at power-on |

The split in `fx3/firmware/` mirrors the one `fpga-register-map.h` and `fpga-registers.h` already have, and for the same reason. `update-protocol.c` includes no SDK header, so it compiles and runs on a build machine — and the arithmetic that decides where each byte lands, on either medium, is exactly the sort that fails quietly on hardware. It carries the boot block's encoder too, checked byte for byte against the one `fpga/make-boot-block.py` writes, because a device and a build tool that disagreed about those twenty-four bytes would be a unit that booted the wrong half of its flash. `update-agent.c` and `epcs-flash.c` are the halves that cannot be tested anywhere but a bench.

Related pages: [Update bundle format](update-bundle-format.md), [EPCS layout and boot flow](epcs-layout-and-boot-flow.md), [Developer update loop](developer-update-loop.md), [FPGA register interface](fpga-register-interface.md).
