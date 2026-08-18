# Test mode and integrity checking

A capture of a disc cannot be checked. There is nothing to compare it against — a dropped
sample looks exactly like a moment where the signal happened to do something else.

Test mode solves that by replacing the RF input with a signal that is known exactly. The
gateware generates a ramp in which **every sample is the previous one plus one**, so any
break in it is a sample the capture path lost, and it can be found precisely.

This is what to run when a machine is new to you, when a cable or a port has changed, or
when a capture has gone wrong and you need to know whether the fault is in the hardware or
in the disc.

## Taking a test capture

1. Stop any running stream — the mode cannot be changed mid-stream.
2. Tick **Tools → Test data → Test data mode**.
3. Press **Start capture**.

The file is named `TestData_<timestamp>.ddd.flac`. That is forced rather than defaulted, and
the **Name** field is disabled while test mode is ticked: a file called "Blade Runner side 1"
that turns out to be ramps is a trap that costs somebody an afternoon. The capture's tags
record `DDD_TEST_MODE` as well, so the file says what it is even after it has been renamed.

**Sample rate** applies here as well, and a decimated test capture is a real test. The
gateware generates the ramp downstream of the decimator, so at 20 Msps it is still every
sample plus one — which means the integrity check covers the decimated capture path rather
than being locked out of it.

Run it for as long as you would run a real capture. A short test proves the first minute of
a session, which is not the part that fails.

## The two checks

There are two, and they cover different things. **A capture can pass the first and fail the
second, and that failure is the interesting one.**

### Live, as it arrives

The ramp is checked sample by sample while the capture is running. A break stops the capture
immediately with:

> The device's test pattern did not arrive intact, so something in the capture path is
> corrupting data.

This covers the device, the cable, the USB link and the host's transfer path — everything up
to the point where the samples reach the processing thread.

### Offline, off the disk

Reading the finished file back also covers the FLAC encoder, the filesystem and the drive —
everything between the processing thread and the bytes that will still be there tomorrow.

**Tools → Test data → Analyse test data…** opens on the capture folder, because the file
somebody wants to check is almost always the one they have just taken. It shows progress and can be
cancelled; the verdict is coloured so it reads across a bench.

The dialog reads `.ddd.flac` and `.flac`, and uncompressed 16-bit `.ddd.s16` and `.s16` —
and `.raw`, which is what the old application called the same layout.

## The verdicts

| Verdict | What it means |
| --- | --- |
| **Passed** | The ramp was intact from the first sample to the last |
| **Failed** | The ramp broke, and the message says at which sample offset. Everything from the ADC to the drive is in scope, and that offset is where to start looking |
| **Could not be read** | Not a verdict on the capture — a verdict on the attempt. A file that is not there, an extension this application does not handle, or a decoder error part way through |
| **Cancelled** | You stopped it. No break had been found, which is not the same as there being none |

The analysis also reports the **ramp length** it found — older gateware ramps 0…1023 and
newer 0…1020, so it is discovered rather than assumed. If it reports that the file ended
before the ramp wrapped, the capture was too short to exercise what the test is for. Take a
longer one.

## From a script

The same analysis, from a shell:

```bash
ddd-gui --analyse-test-data TestData_2026-08-16_14-30-00.ddd.flac
```

| Exit code | Meaning |
| --- | --- |
| `0` | The ramp was intact |
| `1` | It broke |
| `2` | The file could not be analysed |

The verdict goes to standard output; "I could not read this" goes to standard error, so a
script collecting results does not end up with a message about its own arguments in the
collection.

The read loop is the same code the dialog uses. That is deliberate: the previous application
had it written out twice, and the two could report different things about the same file.

See [Command line](command-line.md) for the other options, including the note about Windows
having no console attached.

## Where this fits

The offline analysis is step 4 of the capture-integrity procedure in the repository's
`TESTING.md` §5 — the four-hour hardware pass that a build has to survive before it is
called a working capture application. Having it scriptable is what makes that procedure a
gate rather than a ritual.

If a test capture fails repeatedly, that is a fault in the hardware or the cabling rather
than in the recording. Check the USB cable and the port first, and prefer a port directly on
the computer to one behind a hub.
