# Capture formats

The capture application writes **FLAC** — a `.ldf` file — and this is the format you want
unless you have a specific reason otherwise.

## What changed

Earlier versions wrote packed 10-bit data as `.lds`: four 10-bit samples squeezed into every
five bytes. That format is no longer written. It is still *read* — years of captures exist
in it, and the analysis tools in the application open it as before.

| | `.ldf` (now) | `.lds` (before) | `.raw` |
| --- | --- | --- | --- |
| Contents | FLAC-compressed samples | packed 10-bit | uncompressed 16-bit |
| Rate on disk | 25–40 MB/s | 50 MB/s | 80 MB/s |
| An hour of capture | 90–135 GB | 180 GB | 288 GB |
| Read by ld-decode | directly | needs conversion | directly |

How much smaller depends on the signal, which is why the FLAC row is a range: compression
cannot be predicted before there is something to compress. ld-decode's tooling reckons about
half the size of `.lds` for LaserDisc RF. Plan for the pessimistic end of the range when
sizing a drive — the application's own free-space estimate does.

## Why

The decode toolchain already wanted FLAC. `ld-compress`, part of ld-decode, existed to
convert `.lds` into exactly this format after the fact — so every capture was written once,
read once and rewritten once before anything could be done with it. Writing it directly
removes that round trip, and halves what the capture writes to disk on the way.

The samples themselves did not change. ld-decode's own code calls the 16-bit form "the DdD
16-bit format", and it is what the application always computed internally; only the
container is different.

## Reading a `.ldf`

ld-decode and vhs-decode take one directly:

```bash
ld-decode capture.ldf output
```

Ordinary audio tools can read it too, since it is a real FLAC stream — though note it is
**Ogg**-encapsulated, and `flac` guesses encapsulation from the file extension, so it needs
telling:

```bash
flac -t --ogg capture.ldf      # verify the file is intact
```

## Converting back to `.lds`

If something in your workflow still needs the old format, ld-decode's `ld-compress` converts
in both directions:

```bash
ld-compress --uncompress capture.ldf     # produces capture.lds
```

The result is byte-for-byte what the old capture path would have written.

## The uncompressed option

Compression happens *while the capture is running*, which costs CPU at the moment the
application can least afford to fall behind. On a machine that cannot keep up, the capture
reports buffer overruns — and a capture with overruns has lost samples, which is not
recoverable.

Two settings in **Edit → Preferences** deal with this:

- **FLAC Compression** (0–8, default 1). Lower is faster and larger. Reduce this first.
- **Capture Format**, set to *16-bit Signed Scaled* — no compression at all, at the cost of
  writing 80 MB/s instead of 25.

If a capture completes with no overrun warnings, the setting is fine. If it does not, lower
the compression level rather than assuming the machine is unsuitable.

## What is inside the file

Each `.ldf` carries a few Vorbis comments recording where it came from, which survive being
moved away from the `.json` sidecar written alongside it:

| Tag | Meaning |
| --- | --- |
| `DDD_VERSION` | the commit of the application that captured it |
| `DDD_SAMPLE_RATE_HZ` | the real sample rate, 40000000 or 10000000 when decimated |
| `DDD_DECIMATION` | 1, or 4 for a CD RF capture |
| `DDD_TEST_MODE` | 1 if this was a test-pattern capture rather than real signal |
| `DATE` | when it was captured |

Note that the FLAC header's own sample rate reads 40000, not 40000000: FLAC cannot express
a rate above 655350 Hz, so the field is a label rather than a measurement. ld-decode uses
the same convention.

## CD RF capture

4:1 decimation, for CD RF, is a checkbox in Preferences rather than a separate format. It
applies to both formats and produces a 10 Msps capture, with the FLAC header stamped 10000
so the file says what it is.
