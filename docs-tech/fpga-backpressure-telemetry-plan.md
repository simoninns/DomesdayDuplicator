# FPGA Back-Pressure Telemetry (Capture Buffer Instrumentation) — Investigation and Plan

## Purpose

A Domesday Duplicator capture either works or it does not, and today the host has no way
to tell how close to not working it was. The gateware's 16384-word capture FIFO is drained
by the FX3 in whole packets, and the only thing that escapes about its state is a single
`buffer_error` pin — which the firmware turns into an LED pattern and never reports to the
host at all. A capture that peaked at 99% occupancy on every packet and a capture that
never went above half look identical from `ddd-gui`.

The goal of this plan is that `ddd-gui` can show, live and during a capture, **how much
headroom the device actually had**: current FIFO occupancy, the peak since the last poll,
how many times the FIFO overflowed, and how many samples were lost — read out over the
existing SPI register link, with no measurable effect on the capture itself. The
user-facing result is a back-pressure indicator in the Statistics panel that swings between
0 (no pressure) and 100 (samples lost), in the style of the bars already there.

This plan targets `ddd-gui` only. The legacy `gui/` application is being retired before the
next release and gains none of this.

## Authoritative references (in-tree)

- Conventions, the three-component protocol rule, gateware policy: [AGENTS.md](../AGENTS.md)
  (§2 "any FPGA↔FX3↔host protocol change touches three components", §5.3 Verilog rules and
  the byte-identical `.jic` test, §5.3 "a change under `fpga/common/` or `fpga/factory/` is
  a re-provisioning event")
- The freeze policy this plan has to argue against:
  [fpga/factory/README.md](../fpga/factory/README.md)
- The link and the map being extended:
  [docs/content/development/fpga-register-interface.md](../docs/content/development/fpga-register-interface.md),
  [fpga/common/spiRegisters.v](../fpga/common/spiRegisters.v)
- What is being measured: [fpga/application/buffer.v](../fpga/application/buffer.v),
  [fpga/application/fifo.v](../fpga/application/fifo.v),
  [fpga/application/fx3StateMachine.v](../fpga/application/fx3StateMachine.v)
- The firmware transport, and the reason it needs no change:
  [fx3/firmware/src/fpga-registers.c](../fx3/firmware/src/fpga-registers.c),
  [fpga-register-map.c](../fx3/firmware/src/fpga-register-map.c),
  [domesday-duplicator.c](../fx3/firmware/src/domesday-duplicator.c) (`0xB7` handling, and
  the `CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE` channel that carries the samples)
- Host plumbing the figures travel through:
  [ddd-gui/src/capture/monitor_tap.h](../ddd-gui/src/capture/monitor_tap.h) (the wait-free
  rule), [libusb_source.h](../ddd-gui/src/capture/libusb_source.h),
  [winusb_source.h](../ddd-gui/src/capture/winusb_source.h),
  [usb_device.h](../ddd-gui/src/capture/usb_device.h),
  [wire_protocol.h](../ddd-gui/src/capture/wire_protocol.h),
  [gui/statistics_panel.h](../ddd-gui/src/gui/statistics_panel.h)

## What exists today (investigation summary)

### The capture buffer

`buffer.v` holds one 16384-word FIFO with a packet threshold of 8192 words. At 40 MSPS the
whole FIFO is 410 µs of data, of which the 8192 words above the threshold are the headroom
a USB stall is paid out of — about 205 µs.

The steady state matters for reading the numbers this plan produces, so it is worth
stating: `data_available` is raised only once 8192 words are queued and is then held for
exactly one packet, and the FX3 drains at up to one word per 80 MHz clock while the ADC
side writes one word per two. So a **healthy** capture sawtooths between roughly 4096 and
8192 words — 25% to 50% of depth. *Peak occupancy of about 50% is normal and expected.*
Everything above that is the FX3 having been late, and that excursion is the entire signal
this instrument exists to report.

Overflow behaviour: `fifo.v` discards a write that arrives while full, and `buffer.v`
raises `buffer_error` and holds it for 2000 clocks (25 µs) so the FX3 cannot miss it. The
FX3 takes it on a GPIO interrupt and uses it for one thing only —
`FPGA_LED_BUFFER_ERROR` in the application thread's LED logic. Nothing reaches the host.

### The register link

`spiRegisters.v` is an SPI slave running from the same 80 MHz system clock as the capture
path — there is no clock-domain crossing anywhere in this design. Reads of unmapped
addresses return `0x00`, and the map is extended by defining addresses; `0x30`–`0x37`
already sets the precedent this plan follows, being a read-only diagnostic window added at
map version 2 **without bumping the version**, made safe by a signature byte.

The firmware relays the bank to the host with vendor requests `0xB7` (read, `wValue` =
first address, `wLength` = count, maximum 64) and `0xB8` (write one register). Writes are
policed: `fpgaRegisterIsHostWritable()` permits `TEST_MODE` and nothing else.

Link speed is the constraint on how much can be read. The slave tolerates 2 MHz; the
bit-banged master runs at about 100 kHz because each edge is a GPIO register write, so a
byte costs roughly 80 µs.

### The host

`ddd-gui` reads registers today only when a device is noticed — `ReadRegisters()` and
`OpenControlChannel()` in `libusb_device.cpp` both **claim the interface**, so neither can
be used while a capture is running, and `DeviceMonitor` is suspended for the duration of a
capture for the same class of reason. Any polling this plan adds must therefore go through
the handle the streaming source already owns.

`CaptureStats` (in `monitor_tap.h`) already carries `slots_in_use` and `peak_slots_in_use`
for the *host* ring, published through a seqlock so the processing thread never waits on
the GUI. The device-side figures belong in the same block, shown beside them.

## The measurement design

### What is captured

Per polling interval, in a shadow bank that the host reads:

| Figure | Why |
| --- | --- |
| Occupancy now | A live bar; on its own it is nearly meaningless (see the sawtooth above) |
| **Peak since last latch** | The headline number. A 4 Hz instantaneous read samples a 25 ns-resolution signal four times a second and sees nothing |
| Peak since reset | Never cleared, so no reader — or second reader — can hide an excursion |
| Overflow events since last latch | Distinct bursts, not cycles |
| Dropped words since last latch | Samples actually lost; cross-checks against the host's sequence validator |
| Packets read since last latch | The drain rate, from the device's own point of view |
| Samples at or above near-full | Duration, not just amplitude — a single spike and a sustained squeeze look the same in a peak |
| Latch count | Lets a host notice that something else polled and consumed an interval |

### Why the counters must be latched

The link takes about 80 µs per byte and `used_words` changes every 25 ns. Any multi-byte
value read directly off live counters tears. So a read of the block latches every counter
into a shadow bank in one clock, clears the per-interval ones, and the host reads the
shadow.

### Latch on read, not on write — the decision

**Recommendation: reading `TELEM_ID` at `0x40` is what arms the snapshot.** The mechanism
is a one-clock pulse raised when the command byte of a *read* transaction decodes address
`0x40`; the shadow updates on the next clock, roughly 80 µs before the first shadow byte is
loaded into the shift register. `0x40` itself is a constant signature, so the fact that its
own byte is loaded pre-latch does not matter.

Against the alternative — write `0x40` to latch, then read the block:

- **It needs no firmware change.** A write-to-latch register would have to be added to
  `fpgaRegisterIsHostWritable()`, which means the feature needs new gateware *and* new
  firmware, and new gateware paired with fielded firmware would fail with a stall rather
  than degrade.
- **Latch and read become one atomic transaction.** With write-then-read, a second poller
  (a CLI, a second window, a developer with a script) can latch between another reader's
  latch and its read, and the first reader silently receives an interval that is not its
  own. Latch-on-read makes that race structurally impossible.
- **One control transfer per poll instead of two**, halving both the EP0 traffic and the
  bit-banged SPI time spent in the FX3's USB thread.
- **It does not add a second host-writable register.** The map deliberately allows exactly
  one, and the reasoning behind that restriction — recorded at length on the register
  interface page — is worth not eroding for an instrument.

The cost is honest and should be documented as an exception rather than hidden: the access
rules table on the register interface page currently says a read has no side effects, and
after this it has one, at one address. Snapshot-on-read and clear-on-read counters are a
long-standing hardware idiom, and the damage a mistaken read can do here is bounded at one
lost interval — the lifetime peak and the sticky overflow bit are never cleared, so nothing
is permanently concealed. The static geometry registers are placed *outside* the latching
block so a host can read depth and thresholds without disturbing anyone's interval.

## Register block

Added at previously unmapped addresses, self-described by a signature, **no `MAP_VERSION`
bump** — the same rule and the same reasoning as the `0x30` window.

| Address | Name | Contents |
| --- | --- | --- |
| `0x40` | `TELEM_ID` | `0xBD` when the instrument is present, `0x00` otherwise. **Reading this address latches the snapshot.** |
| `0x41` | `TELEM_STATUS` | 3:0 format version (`1`), 4 sticky overflow since reset, 5 a counter saturated this interval, 7:6 reserved |
| `0x42` | `TELEM_LATCH_COUNT` | u8, wraps. Increments on every latch |
| `0x43`–`0x44` | `TELEM_USED_NOW` | u16 words at the latch instant |
| `0x45`–`0x46` | `TELEM_USED_PEAK` | u16, since the previous latch |
| `0x47`–`0x48` | `TELEM_USED_PEAK_ALL` | u16, since reset, never cleared |
| `0x49`–`0x4A` | `TELEM_OVERFLOWS` | u16 saturating, since the previous latch |
| `0x4B`–`0x4C` | `TELEM_DROPPED` | u16 saturating, since the previous latch |
| `0x4D`–`0x4E` | `TELEM_PACKETS` | u16 wrapping, since the previous latch |
| `0x4F`–`0x50` | `TELEM_NEARFULL` | u16 saturating, samples at or above the near-full threshold ÷ 256 |
| `0x51`–`0x52` | `TELEM_DEPTH` | u16, FIFO depth in words (16384). Static — reading it does not latch |
| `0x53`–`0x54` | `TELEM_PACKET_WORDS` | u16, packet size (8192). Static |
| `0x55`–`0x56` | `TELEM_NEARFULL_WORDS` | u16, the near-full threshold (12288). Static |
| `0x57`–`0x7F` | — | unmapped |

Multi-byte fields are least significant byte first, matching the `0x30` window so a host
assembles every block in the same order.

Counter widths, checked against a 250 ms poll at 40 MSPS: packets are 1221 per interval
against a 16-bit wrap at 13.4 s; near-full at ÷256 saturates only after 0.42 s of
continuous near-full; dropped words saturate at 65535, which is 1.6 ms of total loss — at
that scale the exact figure is not the point, and bit 5 of `TELEM_STATUS` says the number
is a floor rather than a count.

The whole poll is one `0xB7` with `wValue = 0x40` and `wLength = 23`, well inside the
64-byte request limit. **The geometry is read on every poll rather than once and
remembered**, which is a change from the first draft of this plan: a remembered figure is a
figure that can belong to a different device, and six extra bytes cost about half a
millisecond of a request that happens four times a second. Total cost is about 1.8 ms of
the FX3's endpoint-0 thread per poll, or 0.7% of it.

## Gateware implementation

### `fpga/application/bufferMonitor.v` (new)

```
module bufferMonitor #(
    parameter integer FifoDepth     = 16384,
    parameter integer NearFullWords = 12288
) (
    input reset_n,
    input clock,
    input [$clog2(FifoDepth+1)-1:0] used_words,
    input write_enable,   // a sample is offered this cycle
    input overflow,       // write_enable && full - buffer.v already derives this
    input is_reading,     // the FX3 took a word this cycle
    input latch,          // one clock: snapshot, then clear the interval counters
    output [127:0] telemetry
);
```

Instantiated inside `buffer.v`, which already has every input to hand and which exports
`telemetry` and accepts `latch` for the top level to wire through. All one clock domain, so
there is no synchroniser and no CDC to get wrong.

**Every port but `latch` is an output, and `latch` reaches nothing but monitor registers.**
There is no path from this module back into the FIFO, its pointers, `data_available` or the
GPIF handshake, so a defect in it cannot corrupt a capture — it can only misreport one.
That property is the reason to prefer a separate module over counters bolted into
`buffer.v`, and it should be stated in the module header where a reviewer will see it.

Cost is on the order of 300 logic elements of the device's 22320 (about 1.5%), and the
comparators against `used_words` at 80 MHz are not a timing risk. Confirm both against the
Quartus fitter and TimeQuest reports rather than asserting them.

### `fpga/common/spiRegisters.v` (extended)

```
    parameter TelemetryPresent = 1'b0;   // application image sets 1; factory leaves 0
    input  [127:0] telemetry;            // 0x41 to 0x50, LSB first
    input  [ 47:0] telemetry_geometry;   // 0x51 to 0x56, static
    output         telemetry_latch;      // one clock on a read command addressing 0x40
```

`read_register()` gains the cases, each gated on `TelemetryPresent` so the whole window
constant-folds to `8'h00` when the parameter is zero. The latch pulse is raised in the
command-byte branch — `read_transfer && shift_in_next[6:0] == 7'h40` — and deliberately
**not** on an auto-incremented address that happens to pass through `0x40`, so a read that
starts at `0x3F` does not latch. The module keeps its stated identity: a shift register and
a register bank, with the counting done elsewhere.

### `fpga/application/DomesdayDuplicator.v`

Wires `buffer_0`'s telemetry bus and geometry into `spi_registers_0`, sets
`TelemetryPresent(1'b1)`, and connects `telemetry_latch` back to the buffer.

### `fpga/factory/DomesdayDuplicatorFactory.v`

Leaves `TelemetryPresent` at its default of zero and ties the buses to zero; the factory
image has no capture path and nothing to report. `0x40` reads `0x00` there, which is exactly
what it reads today, so a host correctly concludes the instrument is absent.

### The `common/` change and the re-provisioning policy

This touches `fpga/common/`, which `fpga/factory/README.md` makes a re-provisioning event
requiring a written justification in the PR: what changed, why it could not wait, and what
happens to units that are never re-provisioned.

The answer to the third question is **nothing**. The factory image gains a window that is
parameterised off; its behaviour is unchanged in every observable respect, including the
gateware update path that runs over this same link. A fielded unit that never sees a JTAG
cable continues to boot, recover and be updated exactly as before, and pairs with the new
application image without qualification.

That claim is testable rather than merely arguable, and AGENTS.md §5.3 gives the test:
build the factory image before and after the change and compare them byte for byte. If the
disabled window folds away entirely and the images are identical, the deployed factory
image already *is* the post-change image and this is a re-provisioning event on paper only.
**Run that comparison as an explicit phase gate**, and if the images differ, say so in the
PR and describe the difference rather than quietly shipping it.

**Result: they are identical.** Compare the *configuration data*, not the `.sof` — a `.sof`
carries a compile timestamp, a per-run design hash and the checksum over them, so two
builds of the same source differ in about thirty bytes of header, as `fpga/package.nix`
already records. Converting each `.sof` with `quartus_cpf` and comparing the raw
bitstreams is the comparison that means something:

```
$ quartus_cpf -c DomesdayDuplicatorFactory.sof factory.rbf     # before, after, and
$ sha256sum *.rbf                                              # before rebuilt again
9586d6f2…  before.rbf
9586d6f2…  before2.rbf
9586d6f2…  after.rbf
```

The fitter agrees: 1,194 logic elements and 656 registers on both sides. The factory image
that is in fielded units is bit-for-bit the image this change produces, so no unit needs
re-provisioning and none is left behind — which is the answer the freeze policy asks for,
supported by evidence rather than by argument.

## Why this cannot disturb a capture

Four places where it could, and what bounds each:

**The gateware.** Read-only taps into registers of their own; the only input to the capture
region is a latch pulse confined to monitor state. Worst case is a wrong number, not a
wrong sample.

**The FX3.** No firmware change at all. One extra `0xB7` per poll costs 18 bytes at roughly
80 µs each — about 1.4 ms of `CyU3PBusyWait` in the USB thread, or 0.6% duty at 4 Hz. The
sample path is a `CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE` channel, so the CPU never touches
sample data and a busy-wait cannot drop one. `glFpgaLock` already serialises SPI against the
application thread's LED writes; the only consequence of contention is an LED updating a
couple of milliseconds later.

**The USB link.** 17 bytes on EP0 against roughly 320 Mbit/s of bulk on a 5 Gbit/s link.

**The host — the one that needs care.** The poll must be issued on the handle the streaming
source already holds, because the interface is claimed and `OpenControlChannel()` claims it
too. On libusb, fill and submit an async control transfer from inside `LibUsbSource::Run()`
so the existing event loop reaps it; never call the synchronous API from another thread,
where it would contend with a completion callback that is explicitly allowed to block. Even
if a poll did stall the event loop for its full 1.4 ms, submitted transfers continue to be
filled by the kernel and only reaping pauses — about 115 KB at 80 MB/s, against the 12 MB
of transfers the source keeps in flight. On WinUSB the same reasoning permits an overlapped
control transfer alongside the bulk requests in `AwaitTransfer()`'s wait.

## Host implementation

1. **`wire_protocol.h`** — addresses, signature, field offsets. A deliberate second copy of
   the gateware's numbers, per AGENTS.md §2; this is protocol, not shared code.
2. **`capture/fpga_telemetry.{h,cpp}` (new)** — `FpgaTelemetry` and
   `ParseFpgaTelemetry(std::span<const uint8_t>)`, plus derived interval figures (peak as a
   percentage of depth, drop rate, whether an interval was stolen by comparing latch
   counts). Pure, no libusb, unit-testable with nothing plugged in — the same shape as
   `fpga_version.cpp`, and for the same reason.
3. **Publication** — a seqlock publisher owned by the source, exactly as `monitor_tap.h`
   requires: the capture side never waits on a reader. The pipeline folds the latest
   snapshot into `CaptureStats` when it publishes, so every existing consumer gets it for
   free.
4. **Polling policy** — 4 Hz while a source is running, and **only** while one is running.
   With no host draining, the FIFO sits permanently full and overflowing; idle telemetry
   would be alarming and meaningless. The panel shows "—" when nothing is streaming, and
   the first poll after a source starts should be discarded, since its interval covers the
   idle period before the drain began.
5. **`StatisticsPanel`** — a back-pressure indicator, specified in full below.
6. **Logging** — one message the first time peak crosses the near-full threshold in a run,
   and a message per interval that reports any overflow, with the dropped-word count.
7. **`CaptureProvenance`** — recording peak occupancy and total overflows for the run
   would let an archived capture carry evidence of how close it ran. **Deferred, and not
   for want of value:** the provenance tags are built in `capture_controller.cpp` and
   handed to `FlacSink::Open()` before a single sample has arrived, because a Vorbis
   comment block is written at the head of the file. A whole-run peak is not knowable
   then. Carrying it would mean either rewriting tags at `Finish()` or putting the figure
   in a sidecar, and both are a change to the capture file format rather than a change to
   this instrument. What the run leaves behind instead is a log line, below.
8. **The end-of-run log line** — the device's half of the summary the pipeline already
   writes: peak back pressure, peak occupancy, overflows and samples dropped. This is the
   figure nobody thinks to look for until a later capture fails, so every run states it
   whether or not anything went wrong.

## The back-pressure indicator

A bar in the Statistics panel that swings between **0 — no pressure** and **100 — fail**,
built as a `QProgressBar` in exactly the style of the existing buffer-queue bar
(`statistics_panel.cpp:69-77`): `setRange(0, 100)`, added with `form->addRow()`, its caption
carried in `setFormat()`, a tooltip explaining what it means, and an object name
(`kBackPressureBarName = "statistics_back_pressure"`) so a widget test can find it. No
custom painting and no stylesheet, so it inherits the theme like every other bar in the
panel.

It sits directly above the existing "Buffer queue" row. The two bars then read as the
back-pressure chain in physical order — *FPGA FIFO → USB → host ring → disk* — and a user
can see at a glance which stage is the one struggling, which the application currently
cannot answer at all.

### What the bench changed about this

**The first version of this scale was wrong, and the hardware said so.** It is recorded
here rather than quietly fixed, because the mistake is one that any "0 means healthy"
indicator invites.

The bar was drawn from the back-pressure figure below: zero until the buffer rose above
the packet threshold. On real hardware the peak is **8194 words** — two above the 8192
threshold — for hours at a time, so the bar sat at zero for the whole of every healthy
capture. Every number was correct and the display was useless: a bar that never moves
cannot be told from a bar that is broken, and the first thing anyone asked on seeing it
was whether the FPGA's buffer was being used at all.

So the bar is now **how full the buffer got, as a fraction of the whole buffer**, and the
back-pressure figure is what the caption, the log and the run summary judge by. On this
scale the halfway mark is where ordinary use ends, which the tooltip says in as many
words. Streaming from a device, four readings a second:

```
read   peak     now      packets  bar   press  caption
2      8194     4675     1224     50    0      now 4675, peak 8194 of 16384
4      8194     4681     1224     50    0      now 4681, peak 8194 of 16384
6      8194     5557     1224     50    0      now 5557, peak 8194 of 16384
8      8194     6271     1224     50    0      now 6271, peak 8194 of 16384
```

The bar sits at half, the occupancy at the instant of each reading moves, and 1,224
packets per reading is the plainest possible evidence that the GPIF is switching buffers.
None of that was visible before, and all of it was already being measured.

**The occupancy leads the caption and the peak follows it**, which is the second thing the
bench corrected. The peak of a capture that is keeping up is not a measurement that varies:
the FIFO fills to the 8192-word packet threshold, `data_available` is raised on the next
clock, and the FX3 begins draining a cycle or two later while the sampler is still adding a
word every second clock — so the peak overshoots to 8194 and stops there, on every interval,
indefinitely. Leading with it made a working display look frozen. It is still shown, because
the moment it *stops* being constant is the whole point of the instrument, and it is what
the bar and the reserve figure are drawn from.

### The scale, and why it is not raw occupancy

Raw FIFO occupancy would be a bad indicator: a perfectly healthy capture sawtooths between
25% and 50% of depth, so a raw bar would sit permanently at half scale and its top half
would be the only part that ever meant anything. What the user needs is *how much of the
grace period was consumed*, and the grace period is the 8192 words above the packet
threshold — the 205 µs `buffer.v` describes as what a USB stall is paid out of.

So:

```
    pressure = (peak_words - packet_words) / (depth - packet_words) * 100
```

clamped to 0…100, taken from `TELEM_USED_PEAK` and normalised by the geometry the device
itself reports at `0x51`–`0x56` rather than by constants compiled into the host.

The endpoints are then literal rather than decorative:

- **0** — the FIFO never rose above the packet threshold in this interval. The FX3 took
  every packet as soon as it was offered; there was no back pressure at all.
- **100** — the FIFO reached depth, which *is* overflow: samples were dropped. Any interval
  reporting a non-zero `TELEM_OVERFLOWS` is pinned to 100 regardless of the arithmetic, so
  the top of the scale always means the same thing.
- **75** — the near-full threshold, three quarters of the headroom gone.

This figure is what the *caption*, the log threshold and the run summary use. It is not
what the bar is drawn from, for the reason given above: it is zero throughout a healthy
capture, which is the right thing for a judgement to say and the wrong thing for an
instrument to show.

### Behaviour

- Driven from **peak since the last latch**, not instantaneous occupancy: an
  instantaneous read four times a second samples a 25 ns signal and shows noise. Each poll
  therefore contributes the worst moment of its own 250 ms, which is what makes the bar
  move meaningfully rather than flicker.
- **Peak-hold with decay**, as a VU meter does: the displayed value is
  `max(new_sample, displayed * 0.7)`, so a spike is visible for about a second before
  falling back. Without it, a single bad interval at 4 Hz is a 250 ms flash a user will
  miss.
- **Caption** in `setFormat()`, in words and not percentages, because the words are what
  say the buffer is being used: `"8194 of 16384 words  (now 5455)"`. Above the packet
  threshold it becomes `"12500 of 16384 words — 53% into the reserve"`, and once samples
  have been lost it becomes the damage — `"4 overflows, 1182 samples lost"` — because at
  that point no percentage is the interesting number.
- **Tooltip**, rewritten on each reading with what the caption has no room for: the
  lifetime peak, the packet threshold in words, the packets taken since the last reading,
  and the run's overflow and loss totals.
- **Idle** — value 0, caption "—", matching `ShowIdle()`. Nothing is draining the device
  when no source is running, so the FIFO is permanently full and any figure would be a
  lie.
- **Gateware without the instrument** — value 0, caption "not reported by this gateware".
  An older device must not look like a flawless one; this is the same distinction
  `FpgaVersion` already draws between "absent" and "fine".
- Colour changes above a threshold are deliberately **not** proposed, so the bar stays in
  the panel's existing style. If they are wanted later they must come from
  `theme_color_tokens.h` rather than a hardcoded stylesheet.

### Where the logic lives

In `PresentStatistics()` and its `StatisticsView`, as `back_pressure_percent` (int) and
`back_pressure` (QString) — not in the widget. That is the existing split and it exists for
exactly this case: a bar that reads 12% when the device was at 80% is a defect no
screenshot reveals and no widget test reaches without a QApplication, while a percentage
produced by a pure function from a `CaptureStats` value can simply be checked. The
peak-hold decay is presenter state, seeded from the view's previous value, so it is
testable too.

## Testing

- **Gateware** — `fpga/tests/tb_bufferMonitor.v`, added to the explicit bench list in
  `run-sim.sh` (`tb_bufferMonitor:application/bufferMonitor`): peak tracking across a
  latch, saturation behaviour, overflow and dropped-word counting against a stalled reader,
  and that `latch` never perturbs the live counters' inputs. Extend `tb_spiRegisters` for
  the new window and, specifically, that the latch pulse fires on a read command addressing
  `0x40` and *not* on an auto-incremented pass through it. `tb_buffer` gains the new file
  in its DUT list. `run-lint.sh` and `run-style.sh` must pass with no new waivers.
- **Host** — parser unit tests including the absent-instrument and all-ones cases;
  presenter tests for the back-pressure scale, covering both endpoints, the pinned-to-100
  overflow case, the idle and absent-gateware captions, and the peak-hold decay; a pipeline
  test driving a synthetic source that produces telemetry, so the panel is exercised with
  nothing plugged in.
- **Firmware** — none. That is the point of latch-on-read.
- **Bench** — the acceptance criterion worth having: run a capture with a deliberately
  slow sink and confirm that the gateware's dropped-word count matches the gap the host's
  sequence validator reports. That cross-checks both instruments against each other, and
  neither can be trusted alone. Also confirm a healthy capture peaks near 50% and not
  above, which validates the sawtooth reasoning this whole display is scaled around.
- **Factory image** — the byte-identical `.jic` comparison described above, run before the
  PR is opened.

## Implementation phases

1. **Agree the block** — this document; the register layout is the contract three
   components will copy.
2. **Gateware** — `bufferMonitor.v`, the `spiRegisters.v` window, both top levels,
   testbenches, lint and style.
3. **Factory gate** — build the factory `.jic` before and after; compare.
4. **Host parsing** — `wire_protocol.h`, `fpga_telemetry.{h,cpp}`, unit tests. No device
   needed, and nothing user-visible yet.
5. **Host polling** — libusb and WinUSB sources, the publisher, `CaptureStats`.
6. **GUI** — the back-pressure indicator, logging, provenance.
7. **Hardware verification** — bitstream build, the bench criteria above, TESTING.md §5
   re-verification.
8. **Documentation** — a "Capture buffer telemetry" section on the FPGA register interface
   page, including the read-side-effect exception to the access rules table and the note
   that `MAP_VERSION` deliberately does not bump; a TESTING.md entry for the bench check.

## Open questions

- **Poll rate.** 4 Hz, fixed in both backends. Worth exposing in settings?
- **Provenance.** Deferred for the reason above. Worth doing as a sidecar, or as tags
  rewritten at `Finish()`, or not at all?
- **Near-full threshold.** Fixed at 12288 (75%) in this plan. Making it host-settable would
  need a writable register and therefore a firmware whitelist change, which is precisely
  the cost latch-on-read was chosen to avoid. Recommend fixed, reported via
  `TELEM_NEARFULL_WORDS` so the host never hardcodes it.
- **WinUSB polling shape.** Overlapped control transfer inside `AwaitTransfer()`'s wait is
  the proposal; confirm against the driver's behaviour when a control request is queued
  alongside outstanding bulk reads.
- **Peak-hold decay.** 0.7 per poll at 4 Hz — a spike stays visible for about a second.
  Worth tuning on hardware once there is something real to watch.
- **Second readers.** `TELEM_LATCH_COUNT` lets a host detect that something else consumed
  an interval. Is anything other than `ddd-gui` expected to poll — an `update-cli`
  diagnostic mode, for instance?
