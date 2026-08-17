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

The register address post-increments after each data byte, so the identity block is one thirteen-byte transaction: a command byte and twelve data bytes. `BRIDGE_DATA` at `0x22` is the single exception and is described with the register map below.

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

**One read has an effect, and it is the only one.** A read transaction commanded at `TELEM_ID` (`0x40`) samples the capture buffer instrument into its shadow registers and clears its interval counters. Nothing else in this map changes anything when it is read, and that exception is confined to the instrument's own registers — it reaches no other register, no output pin, and nothing in the capture path. The reasoning, and the alternative that was rejected, are under [Capture buffer telemetry](#capture-buffer-telemetry-0x40-to-0x56) below.

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

Map version `0x02`, which is what both gateware images in this repository report. Reads of unmapped addresses return `0x00`, so the map is extended by defining addresses, and `MAP_VERSION` tells a reader which definitions to trust.

| Address | Name | Access | Reset | Host-writable |
| --- | --- | --- | --- | --- |
| `0x00` | `ID` | RO | — | — |
| `0x01` | `MAP_VERSION` | RO | — | — |
| `0x02` | `BUILD_FLAGS` | RO | — | — |
| `0x03` to `0x0A` | `COMMIT_0` to `COMMIT_7` | RO | — | — |
| `0x0B` | `IMAGE_ROLE` | RO | — | — |
| `0x0C` to `0x0F` | — | unmapped | | |
| `0x10` | `TEST_MODE` | RW | `0x00` | yes |
| `0x11` | `LED` | RW | `0x01` | no |
| `0x12` | `DECIMATION` | RW | `0x01` | yes |
| `0x13` to `0x1F` | — | unmapped | | |
| `0x20` | `BRIDGE_UNLOCK` | RW | `0x00` | no |
| `0x21` | `BRIDGE_CONTROL` | RW | `0x00` | no |
| `0x22` | `BRIDGE_DATA` | RW | — | no |
| `0x23` | `RECONFIG_CONTROL` | RW | `0x00` | no |
| `0x24` to `0x2F` | — | unmapped | | |
| `0x30` to `0x37` | `RU_DIAG_0` to `RU_DIAG_7` | RO | — | — |
| `0x38` to `0x3F` | — | unmapped | | |
| `0x40` | `TELEM_ID` | RO | — | — |
| `0x41` to `0x50` | `TELEM_STATUS` to `TELEM_NEARFULL` | RO | — | — |
| `0x51` to `0x56` | `TELEM_DEPTH` to `TELEM_NEARFULL_WORDS` | RO | — | — |
| `0x57` to `0x7F` | — | unmapped | | |

Version 1 changed nothing below `0x0B`, and the identity block at `0x00` to `0x0A` is frozen across all map versions, so a host that does not recognise the version can still read who it is talking to. `0x20` to `0x23` are the flash bridge and the reconfiguration control, through which the FX3 reaches the EPCS configuration flash and triggers reconfiguration; they are defined on the [device update mechanism](device-update-mechanism.md) page and summarised below. `0x30` to `0x37` are a bench instrument: the reconfiguration block's read-back of its own setup, added at map version 2 without bumping it, because read-only registers at addresses that used to read zero break nothing. `0x40` to `0x56` are the capture buffer instrument, added the same way and for the same reason, and present only in the image that has a capture buffer.

"Host-writable" is a firmware policy, not a gateware one. The gateware accepts a write to any read/write register from whoever is on the link; the FX3 is what declines to relay some of them.

**`TEST_MODE` and `DECIMATION` are the only host-writable registers, and the flash bridge is the reason that matters.** Both of them select what the capture path does with the samples before they reach the buffer, both are meaningless to the firmware, and the host is the only thing that knows which the user asked for. A new one of these is a firmware change as well as a gateware change: `fpgaRegisterIsHostWritable()` is a list of addresses, and a write to an address it does not name is refused with a stall however willing the gateware would have been. `0x20` to `0x23` are refused as firmly as the LED register and for a stronger reason: the firmware owns the bridge during an update, and a host writing to `BRIDGE_DATA` between two of the firmware's own writes would shift an unaccounted byte into a flash command in progress. The bridge's four-byte unlock is what stands between a *stray* write and an unbootable board; refusing to relay the write at all is what stands between a deliberate one and the same result. Everything a host legitimately wants from the bridge — write this gateware, reload the FPGA — it asks for through `0xD1`–`0xD3` and `0xD5`, where the firmware is the one holding the sequence.

### Identity block, `0x00` to `0x0A`

Eleven contiguous read-only bytes, designed to be read in one transaction — twelve with `IMAGE_ROLE` beside them, which is how a host reads who it is talking to and which image is answering in a single go.

**`ID`** is the fixed value `0x44`. This is the only means of detecting whether a gateware register bank is present at all. SPI has no acknowledgement, so an absent or unconfigured FPGA does not fail a transfer; it returns whatever the `FPGA_MISO` line happens to carry. The two things that line can plausibly carry are all-ones, if the pin floats or is held by the Cyclone IV's configuration-time weak pull-up, and all-zeros if it is pulled down. `0x44` is neither, and that is the point of the value.

**`MAP_VERSION`** is the register map version, `0x02` for this document. A reader that does not recognise the value must use only the identity block, which is frozen for all future map versions.

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

### `IMAGE_ROLE`, `0x0B`

`0x00` for the factory image, `0x01` for the application image.

The gateware is two bitstreams and only one of them can capture, so "which image am I running?" is a question that needs an answer rather than an inference. A host reading `0x00` is talking to a unit in recovery: the register interface answers, the flash bridge works, and there is no capture path at all. The two-image model is on the [EPCS layout and boot flow](epcs-layout-and-boot-flow.md) page.

### `TEST_MODE`, `0x10`

Non-zero selects the test data generator in place of the ADC; zero selects the ADC. The reset value is `0x00`, so a gateware that has just come out of reset captures real samples.

Any non-zero value means on. The register is a byte rather than a bit so that a future mode can be a value rather than a flag, and so that a host writing 1 and a host writing `0xFF` agree about what they asked for.

Changing this mid-capture is permitted and takes effect at the next sample, but the sample stream will contain the discontinuity. The application sets it before starting a capture.

### `DECIMATION`, `0x12`

How many device samples each sample the host receives stands for. `0x01` is every sample — 40 Msps, the reset value and what a LaserDisc capture uses. `0x02` halves it to 20 Msps, which is enough for tape RF and half the file.

**This is not "send every second sample".** Halving the rate without filtering first folds everything above 10 MHz down on top of the signal: a 15 MHz component would reappear at 5 MHz, directly on top of a tape's luma FM carrier, and nothing downstream could tell the alias from the signal. So the gateware low-passes the stream at 10 MHz before it decimates, with a 63-tap half-band FIR — ±0.0015 dB of passband ripple to 8 MHz, 75 dB or better of rejection from 11.4 MHz upwards, and exactly constant group delay. [The decimation filter](fpga-decimation-filter.md) covers the design, the coefficients, the measured response and the phase.

What no half-band can do is protect the band edge. The response is antisymmetric about 10 MHz and passes exactly −6 dB there, so energy just above 10 MHz still aliases to just below it at a comparable level. That is a property of 2:1 decimation rather than of this filter, and the remedy is to capture a signal with content up there at the full rate.

**The register holds the factor, not a flag**, so reading it back is a statement of what the capture path is doing rather than an echo of what was asked for — and so that a third factor can be a value rather than a second bit. A factor this gateware does not implement is normalised to `0x01` rather than stored, and so is `0x00`, which is not a factor at all.

Only the application image implements it. The factory image has no sample stream to decimate, so its `spiRegisters` is compiled with the register parameterised off and `0x12` reads `0x00` there, exactly as an unmapped address does.

The decimator sits **in front of** the test-data generator and the sequence counter, which is what keeps a decimated capture checkable: the counter is attached to the samples that survive, so the stream carries an unbroken count, and a test-mode capture is an unbroken ramp at whichever rate is selected. Decimating after the generator would drop every second sequence number and every capture would read as damaged.

Changing this mid-capture is permitted and takes effect at the next sample, but the sample stream will contain the discontinuity. The application sets it before starting a capture, alongside `TEST_MODE`.

### `LED`, `0x11`

Bit *n* drives `LED[n]` on the DE0-Nano; a set bit lights the LED. The reset value is `0x01` — LED 0 lit, the rest dark.

The reset value is not cosmetic. From power-on until the FX3 first writes this register the board shows exactly one lit LED, which distinguishes "gateware configured and running, FX3 has not yet spoken to it" from "FPGA unconfigured" — no LEDs, since the pins are high-Z — and from whatever pattern the firmware later chooses. That is three distinguishable states on a board whose only other diagnostic is a UART header.

The gateware generates no pattern of its own. What the LEDs mean once the FX3 owns them is firmware policy:

| Pattern | What you see | Meaning |
| --- | --- | --- |
| `0x01` | One LED at one end of the row | Gateware running, FX3 has not written here yet |
| `0x81` | The two LEDs at the ends of the row, the six between them dark | Enumerated, register link up |
| `0xFF` | All eight lit | The host is collecting |
| `0x55` | Alternating, four lit | The FPGA reported a buffer overflow |
| `0x18` | The middle pair | The firmware is rewriting its own boot EEPROM |

The "what you see" column is worth having because the byte is the thing the firmware writes and the row of lights is the thing anyone is ever looking at, and reading one off the other at a bench needs to know which end of the row is `LED[0]`. Every description above is written so it does not matter — none of them distinguishes a pattern from its mirror image.

Each pattern is chosen to be unmistakable for the others across a room rather than merely different as a number. The update pattern is the one of these a user is most likely to be looking at while wondering whether to unplug something, which is why it has a pattern of its own rather than sharing "collecting", and why it is the only one lit by a pair of centre LEDs.

**The patterns are a priority order, not a set of independent flags.** The application thread picks one each pass of its loop, first match winning: updating, then buffer overflow, then collecting, then ready. It writes the register only when the answer has changed. So an overflow during a capture *replaces* the collecting pattern — `0xFF` giving way to `0x55` part-way through is the fault being reported, not the capture ending.

**What `0x55` can and cannot tell you.** The firmware follows the level on the FPGA's `buffer_error` line rather than latching it: the flag is set on the line's rising edge, and cleared on its falling edge or when a capture starts or stops. The gateware holds that line for 2000 clocks — 25 µs at 80 MHz — restarting the hold on every fresh overflow. The loop that reads it runs every 10 ms.

Those two figures are three orders of magnitude apart, and that decides what the pattern is good for. Overflow that is sustained or repeating retriggers the hold faster than the loop samples, so the LEDs sit at `0x55` and the board is legible from across the room, which is the case the pattern exists for. A *single* isolated overflow will usually have cleared before the next pass looks, so it may never appear on the LEDs at all. **The LEDs are therefore an indication of ongoing trouble, not a record of it.** The record is the buffer monitor's window at `0x40`–`0x56`, which counts overflow bursts and the samples they cost and holds them until read; a capture that shows nothing on the LEDs can still have a non-zero overflow count waiting there.

The gateware will accept a host write to this register, but the FX3 refuses to relay one, because the LEDs are a status output and status outputs have exactly one owner. Two writers means the display shows whichever wrote last, which is worse than useless during a fault — the state the LEDs exist to report is the state where you can least afford to distrust them.

### The flash bridge and reconfiguration, `0x20` to `0x23`

Four registers that reach outside the register bank. They are defined in full on the [device update mechanism](device-update-mechanism.md) page; what belongs here is how they behave as *registers*, because two of them break rules the rest of the map keeps.

| Address | Name | Notes |
| --- | --- | --- |
| `0x20` | `BRIDGE_UNLOCK` | Write the four-byte sequence `0x44 0x44 0x55 0xAA`, one byte per transaction, to unlock the bridge. Any other write to this address locks it again. Reads `0x01` when unlocked and `0x00` when not |
| `0x21` | `BRIDGE_CONTROL` | Write bit 0 asserts the flash's chip select. Reads back bit 0 as that state and bit 1 as busy — a byte shift is in progress. Writes are ignored while the bridge is locked |
| `0x22` | `BRIDGE_DATA` | Write shifts one byte out to the flash and latches the byte that arrived in its place; read returns that latched byte |
| `0x23` | `RECONFIG_CONTROL` | Write bit 0 tickles the configuration watchdog, bit 1 triggers reconfiguration. Reads bit 1 as busy and bit 2 as armed. Bits 7 to 2 of a write are reserved |

**`BRIDGE_DATA` is the one address in the map that does not auto-increment.** It is a port rather than a location, so a multi-byte flash transaction is a run of writes and reads to one address — which is exactly what the post-increment would break. Every other address in the map, including the three beside it, increments as usual.

**The bridge is inert until it is unlocked.** Anything that can send `0xB8` can reach these addresses and the flash holds the only copy of the gateware, so the unlock is what stands between a stray write and an unbootable board. While the bridge is locked the gateware does not drive the flash's pins at all. `nReset` locks it, and so does a completed reconfiguration.

### Remote update diagnostics, `0x30` to `0x37`

Eight read-only bytes carrying one 64-bit word: the reconfiguration block's own account of how the last boot was set up, read back out of the block itself rather than reported by the logic that drove it.

The word is presented **least significant byte first**, so `0x30` is bits 7:0 and `0x37` is bits 63:56 — one eight-byte transaction, and a host assembles it in the order the bytes arrive.

| Bits | Field |
| --- | --- |
| 63:56 | Signature, `0xDD` |
| 55 | All reads completed |
| 54:53 | Master state machine mode — `00` factory, `01` application |
| 52:48 | The trigger condition of the previous configuration attempt |
| 47:24 | The staged boot address, as the block holds it |
| 23:12 | The watchdog timeout value |
| 11 | Watchdog enabled |
| 10 | `Osc_int` — internal oscillator selected |
| 9 | `Cd_early` — early configuration check enabled |
| 8:0 | Reserved, read as 0 |

**The signature is what makes the window safe to read.** An unmapped address returns `0x00`, so gateware built without this instrument reads as all zeros here and is indistinguishable from a working one that has been set up entirely wrongly — which is exactly the case somebody reading these registers is trying to tell apart. `0xDD` in the top byte says the instrument is present; the "all reads completed" bit says it finished.

This exists because the parameters written to the reconfiguration block are otherwise unobservable. They are write-only from the fabric's point of view, they take effect only at the next configuration, and a wrong one produces a device that reconfigures in a loop rather than an error anybody can read. Three of them *were* wrong, and this window is how each was measured rather than guessed — the story is in TESTING.md §6 and on the [EPCS layout and boot flow](epcs-layout-and-boot-flow.md) page.

It is kept rather than removed after the diagnosis, at a cost of a few microseconds of reads during boot and no runtime cost at all. The values are sampled once, immediately after the block is set up, and then stand still.

**`MAP_VERSION` does not bump for this.** Adding read-only registers at previously unmapped addresses is additive under the rule on the [device update mechanism](device-update-mechanism.md) page: an older host reads the addresses it knows and never asks for these, and a host that does ask a gateware without them gets zeros and the missing signature. Both images carry the window, because it lives in `remoteUpdate.v`, which both images carry.

### Capture buffer telemetry, `0x40` to `0x56`

Twenty-three read-only bytes describing the FIFO between the ADC and the FX3: how full it got, how often it overflowed, what that cost, and the dimensions to read those figures against.

This is what lets a host say how close a capture came to failing. The buffer is what a USB stall is paid out of, and before this window the only thing that left the gateware about it was the overflow pin — which says a capture has already been damaged and says nothing at all about the ones that survived.

Only the application image implements it. The factory image has no capture path, so its `spiRegisters` is compiled with the window parameterised off and `0x40` upwards reads `0x00` there, exactly as it did before the window was defined.

#### Reading it samples it

The link moves about a byte every 80 µs and `used_words` changes every 12.5 ns, so a host that read the counters directly would get a different counter's idea of a different instant in every byte. Instead:

**A read transaction commanded at `TELEM_ID` samples every counter into a shadow bank in one gateware clock, clears the interval counters, and increments `TELEM_LATCH_COUNT`.** The host then reads the shadow, which stands still until the next such read.

The rules around that are exact, and each of them is checked by `fpga/tests/tb_spiRegisters.v`:

- Only a **read**, and only one **commanded at `0x40`**. A write there samples nothing, and a read that *arrives* at `0x40` by address auto-increment — one that started at `0x3F`, say — samples nothing either. A transaction that started somewhere else is reading something else and must not silently consume a measurement it never asked for.
- Exactly **once per transaction**, however many bytes it goes on to read.
- The geometry at `0x51` to `0x56` is static and reading it samples nothing, so a host can ask the buffer's dimensions without consuming anybody's interval.

`TELEM_ID` itself is a constant, so it does not matter that its byte is loaded into the shift register before the sample is taken; the first shadow byte is loaded a whole SPI byte later, by which time the sample is eight system clocks old at the fastest link this bank accepts.

**Why a read and not a write.** A write-to-sample register would have been purer — this is the only read in the map with an effect — and it was rejected for three reasons. It would need a firmware change, because `fpgaRegisterIsHostWritable()` is a list of addresses and a register not on it is refused with a stall, so the feature would need new gateware *and* new firmware where a read-only window needs neither. It would take two control requests per reading instead of one. And it would open a race that latch-on-read cannot have: with sampling and reading as separate transactions, a second host on the link can sample between another reader's sample and its read, and hand it an interval that is not its own.

#### The block

Multi-byte fields are **least significant byte first**, the same order the `0x20` and `0x30` windows use.

| Address | Name | Width | Contents |
| --- | --- | --- | --- |
| `0x40` | `TELEM_ID` | 8 | `0xBD` when the instrument is present. **Reading this samples it** |
| `0x41` | `TELEM_STATUS` | 8 | Bits 3:0 layout version (`1`); bit 4 the buffer has overflowed since reset; bit 5 a counter saturated this interval; bits 7:6 reserved |
| `0x42` | `TELEM_LATCH_COUNT` | 8 | Increments on every sample, and wraps. Two readings differing by more than one mean something else sampled in between |
| `0x43`–`0x44` | `TELEM_USED_NOW` | 16 | Occupancy in words at the instant of the sample |
| `0x45`–`0x46` | `TELEM_USED_PEAK` | 16 | The highest occupancy since the previous sample |
| `0x47`–`0x48` | `TELEM_USED_PEAK_ALL` | 16 | The highest occupancy since reset. Never cleared |
| `0x49`–`0x4A` | `TELEM_OVERFLOWS` | 16 | Overflow bursts since the previous sample, saturating |
| `0x4B`–`0x4C` | `TELEM_DROPPED` | 16 | Samples lost since the previous sample, saturating |
| `0x4D`–`0x4E` | `TELEM_PACKETS` | 16 | Packets the FX3 took since the previous sample, wrapping |
| `0x4F`–`0x50` | `TELEM_NEARFULL` | 16 | Samples spent at or above the near-full threshold since the previous sample, in units of 256, saturating |
| `0x51`–`0x52` | `TELEM_DEPTH` | 16 | FIFO depth in words (16384). Static |
| `0x53`–`0x54` | `TELEM_PACKET_WORDS` | 16 | Packet size in words (8192). Static |
| `0x55`–`0x56` | `TELEM_NEARFULL_WORDS` | 16 | The near-full threshold in words (12288). Static |

A stall is **one** overflow event however long it lasts — the run ends at the first sample that finds room again — so `TELEM_OVERFLOWS` answers "how often" and `TELEM_DROPPED` answers "how much". Counters saturate rather than wrap, and say so in the status byte, because a wrapped counter reports a small number for a catastrophe.

The geometry is reported rather than assumed so that a host never carries a copy of this design's dimensions; the capture application reads all twenty-three bytes on every poll for the same reason, since a remembered figure is one that can belong to a different device.

#### Reading the numbers

The shape of a healthy capture is worth stating, because it is not what a full-scale reading would suggest. A packet is offered only once a whole one is queued, and the FX3 then drains at up to one word per system clock while the sampling side writes one word every two — so occupancy sawtooths between roughly a quarter and a half of the FIFO, and the peak lands two words above the packet threshold: **8194 of 16384 words, on every interval, indefinitely.** That is the buffer working, not the buffer struggling.

Everything above the packet threshold is the FX3 having been late, and the room between the threshold and the depth is the 205 µs of grace a USB stall is paid out of. A peak that stops being constant is therefore the reading worth acting on, and `TELEM_NEARFULL` says whether an excursion was a spike or a squeeze.

**`MAP_VERSION` does not bump for this**, on the same rule as the `0x30` window: these are read-only registers at addresses that used to read zero, an older host never asks for them, and a host that asks a gateware without them gets zeros and no signature.

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

The identity block is `wValue` = 0, `wLength` = 12 — eleven bytes of identity and the image role.

### `0xB8` — write register

| Field | Value |
| --- | --- |
| `bmRequestType` | `0x40` — vendor, device, host-to-device |
| `bRequest` | `0xB8` |
| `wValue` | high byte: register address; low byte: value |
| `wIndex` | 0 |
| `wLength` | 0 — no data stage |
| Data stage | none |

Carrying both operands in `wValue` keeps the request to a setup packet with no data stage, which removes the only case where a control transfer here could be partially completed. Turning test mode on is `wValue` = `0x1001`; off is `0x1000`. Selecting 2:1 decimation is `0x1202`, and the full rate is `0x1201`.

### Request validation

The firmware stalls endpoint 0 — the USB way of saying "not supported" — rather than failing quietly, for:

* `0xB7` with `wValue` above `0x7F`, `wLength` of 0, or `wLength` above 64
* `0xB8` naming a register that is not host-writable: every read-only register, `LED`, the four flash-bridge and reconfiguration registers, and every unmapped address
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

The capture application reports all of these in its **Tools → Firmware** dialog and none of them prevent a capture. A version that cannot be read or does not match is a note to the user, never a refusal to work.

## Signal integrity

The lines are push-pull over a few centimetres through two headers, which is unremarkable, but nothing drove a clock down them before this interface existed. If ringing on `FPGA_SCLK` ever proves to double-clock the slave despite the input filter, the remedies in order are: lower the master's clock rate, which costs nothing and is a firmware constant; widen the filter, which costs nothing; and only then reduce the FX3's drive strength, which needs a hardware change.

Series termination is the remedy that is **already present** and is worth knowing about before anyone plans to add it: the SuperSpeed Explorer Kit carries a 22 Ω series resistor on every `CTL` line between the FX3's pin and the GPIF II header, so each of these signals is source-terminated on the FX3 side before it reaches this board. The Duplicator PCB itself adds nothing — the nets run straight from one header to the other.

## Where the code is

| File | Holds |
| --- | --- |
| `fpga/common/spiRegisters.v` | The slave and the registers, shared by both images |
| `fpga/common/flashBridge.v` | `0x20` to `0x22`, and the lock |
| `fpga/common/remoteUpdate.v` | `0x23`, the watchdog, the reconfiguration trigger, and the `0x30`–`0x37` read-back |
| `fpga/application/bufferMonitor.v` | The counters behind `0x40`–`0x56`, and the shadow bank a read samples into |
| `fpga/generate-version.sh` | The build stamp the identity block reports |
| `fx3/firmware/src/fpga-registers.c` | The bit-banged SPI master |
| `fx3/firmware/src/fpga-register-map.c` | The map, and every decision about it that needs no hardware |
| `ddd-gui/src/capture/wire_protocol.h` | The host's copy of the request numbers and addresses |
| `ddd-gui/src/capture/fpga_version.cpp` | Parsing the identity block |
| `ddd-gui/src/capture/fpga_telemetry.cpp` | Parsing the capture buffer block |
| `ddd-gui/src/capture/libusb_source.cpp` | Polling it during a capture, without disturbing one |
