# Settings

**File → Settings…** holds the four things that are set once and left alone. Everything to
do with an individual capture — the folder, the name, the compression, the duration limit —
is in the [Capture panel](capture-control.md) instead, where it is visible while you work.

!!! note "When each one takes effect"

    The buffer and transfer settings resize things a running capture is using, so they apply
    to the **next** capture. The front-end gain declaration only changes what a number is
    labelled, so it applies **at once**, including to a capture already running.

## Buffer queue

How much sample data this machine will hold while waiting for the disk and the encoder.

| Choice | Slack it buys |
| --- | --- |
| 64 MB | 0.8 seconds |
| 128 MB | 1.6 seconds |
| **256 MB** | **3.2 seconds (default)** |
| 512 MB | 6.5 seconds |

The seconds are the figure that matters — the megabytes are just how it is spent. This is
how long a write stall can last before samples start being lost, so raising it is one of the
three remedies for a machine that cannot keep up (the others being a faster drive and a
lower compression level).

On Linux there is a ceiling that is not the application's. The kernel's usbfs memory limit
caps what can be queued, and a queue above it fails the capture with a specific message and
the command to raise it. Lowering the queue size here is the remedy that needs no
administrator rights. See [If a capture fails](if-a-capture-fails.md#the-kernels-usbfs-limit).

## USB transfers

**Many small transfers (recommended)** keeps several reads outstanding at once, so the
device always has somewhere to put the next packet.

**One transfer per buffer** is simpler and slightly cheaper, but leaves a gap between each
transfer completing and the next being submitted — and the device has nowhere to put data
during that gap.

Change it only in response to a specific failure: *this machine did not keep a read request
outstanding* is the message that points here.

## Preferred device

Which Duplicator to open when several are attached. **Whichever is attached** — the default
— is right for almost everyone.

A device in recovery mode can be preferred too. It is the same physical port, and it will be
a capture device again once it has been programmed.

## Front-end gain

This is the one setting worth reading about rather than just choosing.

### Why you have to declare it

The Duplicator's analogue front end is an amplifier whose gain is set by **SW401**, a
four-way DIP switch on the board. The switch is mechanical and has **no electrical path** to
the FPGA or the FX3 — nothing in the sample stream, the USB descriptors or the vendor
requests reveals its position. The application cannot discover it and must be told.

Everything else follows from that one fact:

- **There is no default.** While the setting is undeclared, no display shows a voltage —
  levels read in converter codes, 0 to 1023. A plausible default would put
  authoritative-looking millivolt figures on screen that could be wrong by up to a factor of
  four, with nothing to reveal it. That is worse than showing nothing.
- **Nothing about the capture depends on it.** Samples are stored and written as the codes
  the converter produced. This is a display calibration, applied where the numbers are
  drawn — so getting it wrong and correcting it later re-scales every figure on screen, with
  nothing to redo.
- **Clipping never depends on it.** A clipped sample is one whose code reached 0 or 1023,
  which is a property of the converter. The clip counts stay correct whether the declaration
  is absent, right or wrong.

### Reading the switches

The switch block is written as it sits on the board, left to right: **`1010` is switches 1
and 3 closed, and 2 and 4 open.**

| Switches | Gain | Largest input without clipping |
| --- | --- | --- |
| `1000` | ×8.50 | 235 mV p-p |
| `0100` | ×6.00 | 333 mV p-p |
| `0010` | ×4.40 | 455 mV p-p |
| `1100` | ×4.00 | 500 mV p-p |
| `0001` | ×3.80 | 526 mV p-p |
| `1010` | ×3.34 | 599 mV p-p |
| `1001` | ×3.04 | 658 mV p-p |
| `0110` | ×3.02 | 661 mV p-p |
| `0101` | ×2.79 | 716 mV p-p |
| `1110` | ×2.59 | 771 mV p-p |
| `0011` | ×2.54 | 789 mV p-p |
| `1101` | ×2.45 | 817 mV p-p |
| `1011` | ×2.27 | 879 mV p-p |
| `0111` | ×2.17 | 920 mV p-p |
| `1111` | ×2.02 | 992 mV p-p |

All four switches open is not a setting: with no feedback path the amplifier has no defined
gain. That pattern is what carries the undeclared state.

The right-hand column is the input at which the converter clips outright. The level to
actually aim for is **75 % of it**, which is what the
[Amplitude History](signal-analysis.md#the-nominal-bounds) strip marks — the headroom
between the two is what absorbs the moments a disc is worse than the moment you set the gain
on.

### Once it is declared

Every level display gains a millivolt figure at the BNC: the Statistics panel's signal level
and extremes, the waveform cursor, the amplitude summary. The declaration is also written
into each capture file's tags, so the calibration needed to read those samples as volts
travels with them.

It is remembered between sessions. A gain switch stays where it was put, and asking again
every session for something that has not changed is how a setting ends up ignored.

## What is not here

**Test mode** is in the Capture panel and is deliberately *not* remembered between sessions.
It is a diagnostic, and an application that silently started in test mode because of
something you did last week would produce a capture full of ramps.

## Where settings are kept

| Platform | Location |
| --- | --- |
| Linux | `~/.config/Domesday86/ddd-gui.conf` |
| macOS | `~/Library/Preferences/com.domesday86.ddd-gui.plist` |
| Windows | `HKEY_CURRENT_USER\Software\Domesday86\ddd-gui` |

These are the application's own; the [legacy application](../legacy-gui/index.md) uses a
different identity, so installing one never disturbs the other's settings.

A settings file that has been edited by hand, or written by a different version, is
**clamped** to sensible values rather than rejected — an out-of-range value produces a
working capture rather than a refusal.
