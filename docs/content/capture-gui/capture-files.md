# Capture files

## What is written

Native **FLAC**, with the compound extension **`.ddd.flac`**.

Two extensions rather than one, deliberately: `.flac` is what makes the file open in
ld-decode and in an ordinary audio editor, and `.ddd` in front of it says where the samples
came from. Someone who finds one of these a year later can tell it apart from an audio file
without opening it.

| | |
| --- | --- |
| Container | Native FLAC — **not** Ogg-encapsulated |
| Channels | Mono. A stereo file is not a Domesday Duplicator capture |
| Bit depth | 16-bit, carrying the converter's 10-bit codes centred about zero |
| Sample-rate field | 40,000 Hz — a label, not a measurement. See below |
| Rate on disk | 24–40 MB/s, depending on the signal |
| An hour of capture | Roughly 90–145 GB |

The application writes this format and nothing else. The historical `.lds` (packed 10-bit)
and `.ldf` (FLAC inside Ogg) are neither written nor read here — they were the output of the
capture application this one replaced, which is no longer part of the project.

### The sample-rate label

FLAC's sample-rate field tops out at 655,350 Hz and the device runs at 40,000,000, so the
header carries **40,000** as a label and everything downstream treats it as one. The value
matches what ld-decode's own tooling writes, so a file read by ld-decode is read at the
right speed. The **real** rate is recorded separately in the file's tags.

### How big is it, really

FLAC compression cannot be predicted before there is something to compress, which is why the
figures above are a range. Measured against a noisy 2 MHz tone, one second of capture at a
time:

| Compression level | Size | Of raw |
| --- | --- | --- |
| 0–1 | 34.2 MB | 42.8 % |
| 5 | 24.0 MB | 30.0 % |
| 8 (default) | 23.7 MB | 29.7 % |

The encode cost is almost flat across that range because libFLAC 1.5.0 and later spread it
over several cores — so the higher levels are very nearly free, and over a disc side level 8
saves tens of gigabytes against level 1.

The application's own free-space estimate uses **40 MB/s**, which is the pessimistic end. Do
the same when sizing a drive.

## What it is called

`RF-Sample_2026-08-16_14-30-00.ddd.flac` — the prefix, then the local time the capture
started, dashes rather than colons because a colon is not a legal filename character on
Windows.

Local time rather than UTC because the person who took the capture is the person who will
look for it, and they remember what time it was where they were standing.

Type something in the Capture panel's **Name** field and that is used instead, reduced to
characters that survive a copy to any of the three platforms.

The **Naming…** button beside that field builds the name from what the disc is instead —
`Casper_CLV_PAL_side2_2026-08-16_14-30-00` — and can append the capture's length once it is
known. See [Naming and metadata](capture-naming.md).

A capture taken in [test mode](test-mode.md) is **always** called `TestData_…`. That is
forced rather than defaulted: a file called "Blade Runner side 1" that turns out to be ramps
is a trap that costs somebody an afternoon.

## What the file says about itself

Every capture carries its own provenance in FLAC tags, so the things needed to read it years
later travel with it rather than in somebody's notes:

| Tag | What it holds |
| --- | --- |
| `TITLE` | The name the capture was given |
| `DATE` | When it started, ISO 8601 |
| `ENCODER` | What wrote it |
| `DDD_VERSION` | The commit of the application that produced it. The name is fixed by the file format and is not changing |
| `DDD_FIRMWARE_VERSION` | The commit the Duplicator's FX3 firmware was built from — only when the device said |
| `DDD_GATEWARE_VERSION` | The commit its FPGA gateware was built from, on the same terms |
| `DDD_SAMPLE_RATE_HZ` | `40000000` — the real rate, which the FLAC header cannot express |
| `DDD_TEST_MODE` | Whether this is signal or a test ramp |
| `DDD_FRONT_END_GAIN` | The declared SW401 position — **only when one was actually declared** |

**Three versions rather than one**, because a capture is the product of three builds: this
application, the firmware in the Duplicator's USB chip, and the gateware in its FPGA. The
firmware and the gateware are installed together from one update and come from one commit,
so those two agree unless something went wrong — a half-finished update, or a gateware
built on the bench. The application is released separately and is not expected to match
either of them. When a capture turns out to have something wrong with it, "which build
wrote this" is the first question,
and the gateware is where sample loss, decimation and the sequence markers all live.

A version that is absent means the device did not say: firmware old enough to predate the
embedded commit, an FPGA that had not finished configuring, or gateware older than the
identity register. A gateware built from a tree with uncommitted changes carries a `-dirty`
suffix, the same way the application's own stamp does — a bare commit hash would assert that
a published build produced the file, and for a modified tree that is not true.

The front-end gain row is the other one worth understanding. The tag is written only if a
[front-end gain](settings.md#front-end-gain) was declared: a capture carrying a default gain
figure nobody had checked would look like calibration data and would be wrong, which is
worse than a capture that says nothing and forces the question to be asked.

`DDD_TEST_MODE` is not a nicety either. A test capture and a real one are indistinguishable
by inspection until somebody decodes one.

Read them with any FLAC tool:

```bash
metaflac --list --block-type=VORBIS_COMMENT capture.ddd.flac
```

## The metadata file beside it

Every capture is also written with a `.ddd.yaml` file of the same name, carrying what does
not belong in a tag block: what you typed about the disc, what the player said about itself,
what an examination of the disc measured, and how the capture went.

```
Casper_side2_2026-08-17_14-30-00.ddd.flac
Casper_side2_2026-08-17_14-30-00.ddd.yaml
```

For an uncompressed `.ddd.s16` capture it is the only provenance there is. The full field
reference is on [Naming and metadata](capture-naming.md).

## Reading a capture

ld-decode and vhs-decode take one directly:

```bash
ld-decode capture.ddd.flac output
```

It is a real, native FLAC stream, so ordinary audio tools open it too — including editors
such as Audacity and Tenacity, which cannot open the Ogg-encapsulated `.ldf` at all:

```bash
flac -t capture.ddd.flac      # verify the file is intact
```

The application can also read an uncompressed `.ddd.s16` file back — for the
[test-data analysis](test-mode.md) only.

## If a capture stops badly

**Whatever went wrong, the FLAC stream is closed properly on the way out.** What had already
been written is readable and reports its own length, and the message naming the failure also
names where the partial file is.

See [If a capture fails](if-a-capture-fails.md).

## Where they go

By default, the platform's Movies or Videos folder — a capture is tens of gigabytes of
media, and that is the location a machine's backup rules and disk-space expectations are
already built around. Change it in **File ▸ Settings…**, under **Folder** — it is set once
and then left, which is why it is there rather than on the Capture panel.

On Linux the Flatpak can reach your home directory, `/run/media`, `/media` and `/mnt`
without further configuration; anywhere else needs an explicit grant, which the
[Flatpak page](install-flatpak.md) covers.
