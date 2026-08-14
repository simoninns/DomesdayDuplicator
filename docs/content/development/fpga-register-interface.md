# FPGA register interface

The FX3 configures the FPGA, and reads state back from it, through a register bank in the gateware reached over a private SPI link. The FX3 is the only master and the gateware is the only slave.

This replaced five dedicated configuration lines that carried one bit each, of which only test mode was ever used.

What the interface carries:

* test mode on and off
* the board's status LEDs, driven by the FX3 rather than by the gateware
* a read-only identity block naming the gateware build

What it does not carry, and must never carry: sample data, or anything in the capture path's critical timing. The 16-bit GPIF path — databus, `PCLK`, `dataAvailable`, `readData`, `nReset`, `bufferError` — is untouched by this interface and keeps its own dedicated pins.

## Why SPI and not I2C

An earlier draft of this interface used I2C. On this board SPI is strictly better, and the reasons are worth recording so the decision is not revisited from scratch:

* **No pull-ups exist and none can be added.** The board has no pull-up on any FX3-to-FPGA line, and adding one needs a hardware revision. I2C would have had to rely on the Cyclone IV's internal weak pull-up, specified only as 7 kΩ to 41 kΩ, giving a rise time around 2 µs and capping the bus at roughly 33 kHz. Every SPI line is push-pull, so none of that analysis exists.
* **No open-drain on the FX3.** I2C needed the FX3 to drive SDA open-drain by setting `driveLowEn` without `driveHighEn`. That follows from the SDK's field definitions but is not a documented idiom, and it was the largest unverified assumption in the design. SPI needs only plain outputs and one plain input, which is what the firmware already does with these pins.
* **Far less gateware.** An I2C slave needs START and STOP detection on an asynchronous line, acknowledgement generation, bidirectional control of SDA and explicit recovery rules. An SPI slave is a shift register and two counters, and chip select provides framing for free.
* **The link cannot deadlock.** An I2C slave can hold SCL or SDA low and hang the master, which is why that design needed a timeout on every transaction. An SPI slave drives nothing the master waits on, so a transfer completes in a fixed number of GPIO writes or not at all.

The cost is two extra pins — four rather than two, out of five available. What I2C offered in exchange was multi-drop addressing and byte-level acknowledgement, and neither is worth anything on a dedicated point-to-point link with one slave.

The lost acknowledgement has one consequence, handled below: SPI gives no signal that a slave is present, so the `ID` register is the only presence check and is therefore load-bearing rather than merely reassuring.

## Versioning

Gateware, FX3 firmware and the capture application from one commit ship as a matched set and are flashed together. Mixed versions are *detectable* — that is what the identity block is for — but they are not supported, and no compatibility shims exist for them.

## Physical layer

### Pin assignment

Four of the five former configuration lines carry the link. All are direct point-to-point traces between the GPIF II header (J202) and the DE0-Nano GPIO_1 header (J101) with no series resistors and no components of any kind between them.

| Signal | Direction | FX3 pin | Board net | J202 | J101 | FPGA port | FPGA pin | Former name |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FPGA_SCLK` | FX3 to FPGA | GPIO 22 (CTL_05) | `USB_CTL5` | 21 | 22 | `GPIO1[17]` | `PIN_K16` | `outputE0` |
| `FPGA_MOSI` | FX3 to FPGA | GPIO 23 (CTL_06) | `USB_CTL6` | 19 | 20 | `GPIO1[15]` | `PIN_N11` | `outputD0` |
| `FPGA_MISO` | FPGA to FX3 | GPIO 24 (CTL_07) | `USB_CTL7` | 17 | 18 | `GPIO1[13]` | `PIN_P9` | `outputD1` |
| `FPGA_CS_N` | FX3 to FPGA | GPIO 25 (CTL_08) | `USB_CTL8` | 15 | 16 | `GPIO1[11]` | `PIN_R10` | `outputD2` |

One line is left over. GPIO 26 (CTL_09), board net `USB_CTL9`, FPGA port `GPIO1[09]`, formerly `outputD3`: the FX3 drives it low and the FPGA leaves the pin high-Z. It stays wired and reserved rather than reassigned, because a future need for a fast out-of-band signal — a hardware capture trigger, say — is better served by a dedicated line than by this link.

`FPGA_MISO` is the only one of the four whose direction reverses. That is a firmware configuration change and not a hardware constraint: the firmware already runs CTL-derived GPIOs in both directions.

### Electrical

Every line is push-pull and point-to-point, driven by exactly one device that never stops driving it. There are no pull-ups, no pull-downs, no tri-stating and no possibility of contention, which is the whole reason for choosing SPI.

Two details are fixed in the `.qsf` rather than left to defaults. `FPGA_MISO` is assigned minimum current strength, because the FPGA drives one CMOS input a few centimetres away through two headers and the default 8 mA buys nothing while making overshoot worse. The three FPGA inputs get no pull-up or pull-down assignment, because they are driven at all times the FX3 is running.

### State before the firmware runs

Between power-on and the point where the FX3 firmware configures its pins, the FX3 holds GPIO 22 to 25 high-Z, and the Cyclone IV holds its I/O high-Z with weak pull-ups while it loads from EPCS. `FPGA_CS_N` therefore floats high — deasserted, the safe state — and the slave stays idle regardless of what the other two lines do.

That is not luck; it is why chip select is active low. A design with an active-high select would begin a phantom transaction every time the board was powered on.

## Link protocol

SPI **mode 0**: `FPGA_SCLK` idles low, the slave samples `FPGA_MOSI` on the rising edge, and the slave changes `FPGA_MISO` on the falling edge. All bytes are transferred most significant bit first.

### Framing

`FPGA_CS_N` frames a transaction. Asserting it starts one and returns the slave's bit and byte counters to zero; deasserting it ends one, unconditionally and from any state.

That is the whole of the recovery mechanism, and it is not conditional on anything. A slave that can be left mid-byte by an FX3 that reset at the wrong moment is a slave that needs a recovery protocol; a slave framed by chip select does not. A byte that is incomplete when `FPGA_CS_N` deasserts is discarded, so a partially received data byte is never written to a register.

### Transaction format

Every transaction is a command byte followed by zero or more data bytes:

| Byte | Contents |
| --- | --- |
| 0 | bit 7 — direction: 1 = read, 0 = write; bits 6:0 — register address |
| 1 to n | data |

The register address post-increments after each data byte, so the identity block is one twelve-byte transaction: a command byte and eleven data bytes.

For a **write** the master drives data on `FPGA_MOSI` and ignores `FPGA_MISO`. Writing `0x01` to `TEST_MODE` is the two bytes `0x10 0x01` between chip select edges.

For a **read** the master clocks out dummy bytes, which the slave ignores, and reads the register contents returned on `FPGA_MISO`. During the command byte the slave drives `FPGA_MISO` low, so the first byte a master reads back is always `0x00` and carries no information; register data starts with the second byte.

### Address space and access rules

The command byte gives a 7-bit address, so registers occupy `0x00` to `0x7F`. The address post-increment wraps modulo 128 and does not saturate.

| Condition | Slave behaviour |
| --- | --- |
| Read of a mapped register | Return its value |
| Read of an unmapped address | Return `0x00` |
| Write to a read/write register | Store the value |
| Write to a read-only register | Discard the value |
| Write to an unmapped address | Discard the value |

SPI has no way to refuse a byte, so a bad address cannot be reported in-band and nothing tries to. A host discovers what exists by reading `MAP_VERSION`, which is a positive statement of what the gateware implements, rather than by probing addresses and interpreting silence.

### Timing

`FPGA_MISO` is driven continuously, including while `FPGA_CS_N` is deasserted, when it reads low. With one slave on a dedicated link there is nothing to release the line for, and driving it always means the FX3 never has a floating input.

| Parameter | Value | Notes |
| --- | --- | --- |
| `FPGA_SCLK` maximum frequency | 2 MHz | Slave limit; see below |
| `FPGA_SCLK` minimum high and low time | 250 ns each | |
| Master's actual rate | around 100 kHz | Set by the FX3's GPIO write rate, not by this link |
| `FPGA_CS_N` setup, hold, and gap between transfers | 1 µs | |

The 2 MHz ceiling comes from the slave's input synchronisers: each clock phase must span enough 80 MHz cycles to be seen reliably, and 250 ns is twenty of them. The bit-banged master will not come close, because each edge is a register write through the GPIO block, so the real rate is two orders of magnitude below the limit.

There are deliberately no timeouts anywhere in this interface. The master generates every clock edge, so a transfer takes exactly as long as the master takes to clock it and cannot hang.

### Slave robustness

The slave's three inputs are asynchronous to the 80 MHz system clock, so each passes through a two-flop synchroniser. `FPGA_SCLK` and `FPGA_CS_N` then pass through a filter that accepts a new level only after two consecutive agreeing samples — aimed at ringing on the header connectors, not at metastability, which the synchroniser handles — costing 25 ns against a minimum 250 ns clock phase. `FPGA_MOSI` needs no filter, because it is only sampled at a clock rising edge, half a period after it last changed. `nReset` asynchronously returns the slave to idle and every read/write register to its reset value.

## Register map

Map version `0x01`. Reads of unmapped addresses return `0x00`, so the map is extended by defining addresses, and `MAP_VERSION` tells a reader which definitions to trust.

| Address | Name | Access | Reset | Host-writable |
| --- | --- | --- | --- | --- |
| `0x00` | `ID` | RO | — | — |
| `0x01` | `MAP_VERSION` | RO | — | — |
| `0x02` | `BUILD_FLAGS` | RO | — | — |
| `0x03` to `0x0A` | `COMMIT_0` to `COMMIT_7` | RO | — | — |
| `0x0B` to `0x0F` | — | unmapped | | |
| `0x10` | `TEST_MODE` | RW | `0x00` | yes |
| `0x11` | `LED` | RW | `0x01` | no |
| `0x12` to `0x7F` | — | unmapped | | |

"Host-writable" is a firmware policy, not a gateware one. The gateware accepts a write to any read/write register from whoever is on the link; the FX3 is what declines to relay some of them.

### Identity block, `0x00` to `0x0A`

Eleven contiguous read-only bytes, designed to be read in one transaction.

**`ID`** is the fixed value `0x44`. This is the only means of detecting whether a gateware register bank is present at all. SPI has no acknowledgement, so an absent or unconfigured FPGA does not fail a transfer; it returns whatever the `FPGA_MISO` line happens to carry. The two things that line can plausibly carry are all-ones, if the pin floats or is held by the Cyclone IV's configuration-time weak pull-up, and all-zeros if it is pulled down. `0x44` is neither, and that is the point of the value.

**`MAP_VERSION`** is the register map version, `0x01` for this document. A reader that does not recognise the value must use only the identity block, which is frozen for all future map versions.

**`BUILD_FLAGS`**:

| Bit | Meaning when set |
| --- | --- |
| 0 | Dirty — built from a tree with uncommitted or untracked changes |
| 1 | Commit valid — the commit bytes name a real commit |
| 2 to 7 | Reserved, read as 0 |

Bit 1 is positive logic on purpose. Every "I do not know" case — built outside a git checkout, an older map, a byte that arrived as zero — then reads as *invalid* rather than as a confident claim about commit `00000000`.

**`COMMIT_0` to `COMMIT_7`** are the gateware's git commit as ASCII characters, padded with `0x00` if it is shorter than eight characters.

ASCII rather than a packed 32-bit value, because the commit is not always eight characters. The FX3's CMake asks git for `--short=8`, but a Nix build passes `self.shortRev`, which is seven — the same commit, stamped two lengths — and the capture application already compares the two on their common prefix for exactly this reason. A fixed 32-bit field would have to either invent a digit or lose one; eight bytes of ASCII represent both lengths exactly, and hand the reader the string it wanted without parsing.

When the commit is not known all eight bytes are `0x00` and the commit-valid bit is clear. A dirty build still reports its hash, with the dirty bit set. The bytes are always a hex string, never a word like `unknown`: anything that is not seven or eight hex digits is reported as no commit at all, so a reader never has to decide whether the characters it received were meant to be a hash.

### `TEST_MODE`, `0x10`

Non-zero selects the test data generator in place of the ADC; zero selects the ADC. The reset value is `0x00`, so a gateware that has just come out of reset captures real samples.

Any non-zero value means on. The register is a byte rather than a bit so that a future mode can be a value rather than a flag, and so that a host writing 1 and a host writing `0xFF` agree about what they asked for.

Changing this mid-capture is permitted and takes effect at the next sample, but the sample stream will contain the discontinuity. The application sets it before starting a capture.

### `LED`, `0x11`

Bit *n* drives `LED[n]` on the DE0-Nano; a set bit lights the LED. The reset value is `0x01` — LED 0 lit, the rest dark.

The reset value is not cosmetic. From power-on until the FX3 first writes this register the board shows exactly one lit LED, which distinguishes "gateware configured and running, FX3 has not yet spoken to it" from "FPGA unconfigured" — no LEDs, since the pins are high-Z — and from whatever pattern the firmware later chooses. That is three distinguishable states on a board whose only other diagnostic is a UART header.

The gateware generates no pattern of its own. What the LEDs mean once the FX3 owns them is firmware policy:

| Pattern | Meaning |
| --- | --- |
| `0x01` | Gateware running, FX3 has not written here yet |
| `0x81` | Enumerated, register link up |
| `0xFF` | The host is collecting |
| `0x55` | The FPGA reported a buffer overflow |

The gateware will accept a host write to this register, but the FX3 refuses to relay one, because the LEDs are a status output and status outputs have exactly one owner. Two writers means the display shows whichever wrote last, which is worse than useless during a fault — the state the LEDs exist to report is the state where you can least afford to distrust them.

## USB interface

Two vendor requests act on the register bank directly, so a register added to the map later needs no firmware change to become reachable from the host. They replaced the bit-flag configuration request `0xB6`, which is retired.

### `0xB7` — read registers

| Field | Value |
| --- | --- |
| `bmRequestType` | `0xC0` — vendor, device, device-to-host |
| `bRequest` | `0xB7` |
| `wValue` | starting register address, `0x0000` to `0x007F` |
| `wIndex` | 0 |
| `wLength` | byte count, 1 to 64 |
| Data stage | `wLength` bytes, IN |

The identity block is `wValue` = 0, `wLength` = 11.

### `0xB8` — write register

| Field | Value |
| --- | --- |
| `bmRequestType` | `0x40` — vendor, device, host-to-device |
| `bRequest` | `0xB8` |
| `wValue` | high byte: register address; low byte: value |
| `wIndex` | 0 |
| `wLength` | 0 — no data stage |
| Data stage | none |

Carrying both operands in `wValue` keeps the request to a setup packet with no data stage, which removes the only case where a control transfer here could be partially completed. Turning test mode on is `wValue` = `0x1001`; off is `0x1000`.

### Request validation

The firmware stalls endpoint 0 — the USB way of saying "not supported" — rather than failing quietly, for:

* `0xB7` with `wValue` above `0x7F`, `wLength` of 0, or `wLength` above 64
* `0xB8` naming a register that is not host-writable: every read-only register, `LED`, and every unmapped address
* either request when the start-up probe did not find the register bank
* either request before the application is active

Unmapped addresses are refused on write but permitted on read. Reading one is how a host discovers a register does not exist and gets `0x00`; writing to one is a host asking for something that will silently not happen.

### Start-up probe

The FPGA loads its configuration from EPCS while the FX3 boots and enumerates, and neither waits for the other. The firmware therefore probes for the register bank after the capture path comes up: it reads the identity block, checks `ID` for `0x44`, and retries ten times twenty milliseconds apart before concluding the gateware is not answering. The result is cached, so a host request never waits on retries.

When the probe found nothing, the firmware tries once more every couple of seconds for as long as that remains true. The FPGA is reprogrammed over JTAG while the FX3 keeps running for the whole of gateware development, and without this the board would have to be power cycled as well before the firmware noticed.

A failed probe is logged to the UART console and is **not** fatal. The device stays enumerated and captures normally; only the version report and the LEDs are lost. A device that refused to work because it could not read a version register would be a device whose diagnostics had become the fault.

### What the host sees

| Situation | Result |
| --- | --- |
| No FPGA, or FPGA unconfigured | `0xB7` stalls — the probe read `ID` as `0x00` or `0xFF` |
| Gateware predating this interface | `0xB7` stalls — the pins are high-Z, so `ID` is not `0x44` |
| Gateware with a newer map version | Identity block reads; `MAP_VERSION` unrecognised |
| Working, matched set | Identity block reads; commit matches the application's |

The capture application reports all of these in its **Help → Firmware** dialog and none of them prevent a capture. A version that cannot be read or does not match is a note to the user, never a refusal to work.

## Signal integrity

The lines are push-pull over a few centimetres through two headers, which is unremarkable, but nothing drove a clock down them before this interface existed. If ringing on `FPGA_SCLK` ever proves to double-clock the slave despite the input filter, the remedies in order are: lower the master's clock rate, which costs nothing and is a firmware constant; widen the filter, which costs nothing; and only then reduce the FX3's drive strength or add series termination, which needs a hardware change.

## Where the code is

| File | Holds |
| --- | --- |
| `fpga/src/spiRegisters.v` | The slave and the registers |
| `fpga/generate-version.sh` | The build stamp the identity block reports |
| `fx3/firmware/src/fpga-registers.c` | The bit-banged SPI master |
| `fx3/firmware/src/fpga-register-map.c` | The map, and every decision about it that needs no hardware |
| `ddd-gui/src/capture/wire_protocol.h` | The host's copy of the request numbers and addresses |
| `ddd-gui/src/capture/fpga_version.cpp` | Parsing the identity block |
