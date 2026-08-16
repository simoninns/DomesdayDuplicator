# Capture control

The **Capture** panel is the one that starts things: which device, where the file goes, and
the two buttons.

## Monitoring and capturing

Two buttons, and the difference between them is the whole model.

**Start monitoring** opens the device and runs the stream with no file on the end of it.
The samples are validated, measured and drawn, and nothing is written anywhere. Use it to
set a player's RF output, to check a cable, or to decide whether a disc is worth capturing.
It costs nothing to leave running.

**Start capture** attaches a file to that stream. From idle it starts the stream as well, so
the common case is one press rather than two. From an existing monitoring session the file
is attached at the next buffer boundary and the stream is not interrupted — the device never
knows a capture began.

**Stop capture** detaches the file and finalises it, and leaves the stream running. That is
what makes both sides of a disc possible without reopening the device: stop, turn the disc
over, start again.

**Stop monitoring** ends the stream. It is disabled while a capture is running, because a
capture *is* this stream with a file on the end of it — stopping the stream would end the
recording too, which made this a second, unlabelled stop button sitting directly above the
real one. Stop the capture first; that leaves the stream running, so nothing is lost by
taking the shortcut away.

Both buttons take on a colour while they are doing something, so the state reads from across
a bench rather than only by reading the label: green while monitoring, red while capturing.
The red says *recording* — it is a normal state, not a fault.

## Device

Every Duplicator the application can see, listed by its device path. When only one is
attached — which is almost everyone — there is nothing to choose.

The list says what is wrong with a device rather than hiding it:

| Entry | What it means |
| --- | --- |
| A bare path | A capture device, ready |
| *— recovery mode, no firmware installed* | The device's USB chip has no firmware it will run. It cannot capture, and [Help ▸ Firmware…](if-an-update-fails.md) can program it |
| *— connected at insufficient speed* | It enumerated below SuperSpeed. It is on a USB 2 port or through a hub that is, and cannot carry 80 MB/s |

A device with no firmware is listed rather than hidden. Reporting "no device attached" to
somebody looking straight at one is exactly the moment they most need to be told what to do
next.

The choice is remembered. If you always use the same port, [Settings](settings.md) has a
**Preferred device** that survives restarts.

The list is locked while the stream is running: the device is open, and it cannot be changed
without stopping.

Test mode lives on the **Tools** menu rather than here — it is a diagnostic rather than part
of setting up a capture. The full procedure, and how to check a test capture afterwards, is
on [Test mode and integrity checking](test-mode.md).

## Where it goes

### Folder

Where captures are written. `Browse…` picks one, or type a path.

It defaults to the platform's Movies or Videos folder — a capture is tens of gigabytes of
media, and that is the location a machine's backup rules and disk-space expectations are
already built around.

The folder can be changed while monitoring, which is when somebody is setting up for the
capture they are about to take. It is fixed for the duration of a capture, because the file
is already open.

### Name

Leave it empty and each capture is named after the time it was taken —
`RF-Sample_2026-08-16_14-30-00` — in local time, which is what keeps a folder of captures in
order and matches what the person who took them remembers.

Type a name and it is used instead. Characters that would not survive a copy to another
machine are removed: Windows rejects `<>:"/\|?*`, treats a trailing dot or space as
invisible, and reserves a list of device names. A name that works where it was typed and
fails on a colleague's machine is prevented here rather than discovered later.

In test mode the field is disabled, not merely ignored. A field that accepted text and then
did not use it would be a lie about what the application was going to do.

### Format

| Choice | What you get |
| --- | --- |
| **FLAC — `.ddd.flac`** | Mono 16-bit native FLAC, roughly half the size, carrying the capture's provenance in its tags. The default |
| **Uncompressed — `.ddd.s16`** | The same samples with nothing wrapped round them: signed 16-bit little-endian, no header |

Uncompressed is twice the disk for none of the encoder, which is the trade worth having on a
machine that cannot sustain the encode or when the output is going straight into another
tool. Nothing in the file says what it is, what rate it was written at or which build
produced it — that is the format's nature, and the reason FLAC stays the default.

Both are read back by **Tools → Analyse test data…** and by `--analyse-test-data`.

### Sample rate

| Choice | What you get |
| --- | --- |
| **40 Msps — every sample** | The device's own rate. What a LaserDisc capture needs, and the default |
| **20 Msps — 2:1 decimated, for tape** | Every second sample, halving the rate and the file |

The device always samples at 40 Msps; decimation happens on this side of the USB link, on
the way into the file. Tape RF has a fraction of a LaserDisc's bandwidth, so half the rate
is enough for it and costs half the storage.

There is no filter in front of it — this is plain selection, the same thing the old
application does for its 4:1 CD decimation — so anything above 10 MHz will alias. That is a
decision about the front end rather than about the software.

A decimated FLAC capture carries the rate label for 20 Msps and a `DDD_DECIMATION` tag. A
decimated `.ddd.s16` carries nothing at all, because there is nowhere to put it: write the
rate down.

**Sample rate** is disabled in test mode, where every sample is always kept.

### Compression

FLAC compression, 0 to 8. The default of 8 gives the smallest file and is what a
multithreaded libFLAC sustains at the device's full rate. It is disabled for the
uncompressed format, which has no encoder to ask.

Lowering it is the first remedy for a machine that cannot keep up. The **Encoder backlog**
and **Buffer queue** figures in [Statistics](statistics.md) are what say whether that is the
problem: a backlog that climbs is the encoder, a queue that climbs with no backlog is the
disk.

### Duration limit

Stop automatically after this many minutes, or **No limit**, which is the default. The stop
lands on a buffer boundary, so nothing is half-written. **Reset** puts it straight back to
No limit — a limit tends to be set for one capture, and holding the down arrow back from
forty minutes is forty presses.

The limit is read on every statistics tick rather than latched when the capture starts, so
both it and **Reset** stay live during a capture: deciding halfway through a side that the
limit should go is a reasonable thing to want. It is a length of *time*, so a decimated
capture still runs for as long as it says.

The default is no limit deliberately: a limit that fired in the middle of a side would be
worse than no limit at all.

### Warn below

Warn when the destination volume has less than this much *capture time* left. Ten minutes by
default; **Never** turns it off.

It is a warning and not a refusal, and it is raised once per capture rather than repeatedly.
The estimate is an estimate, and an application that declined to start because it predicted
a shortfall would sometimes be wrong in the direction that costs a session.

### Free space

How much longer this volume will hold a capture, with the byte figure after it.

The time comes first because it is the question people actually have. "412 GB free" does not
tell you whether this will last the side you are about to play; "2:51:40 of capture" does.

The estimate follows the **Format** and **Sample rate** you have chosen: 40 MB/s for FLAC,
which is what one writes on average, 80 MB/s uncompressed, and half of either when
decimating. The same volume therefore holds four times as much 20 Msps FLAC as 40 Msps
uncompressed, and the readout says so.

An unknown figure means the folder does not exist yet, which is an ordinary thing to have
typed on the way to creating it — it is not reported as a full disk.

## Status

The line at the bottom of the panel: *No capture device attached*, *Ready*, *Monitoring*,
or the full path of the file being written. The whole path, not just the name — somebody who
has just started a forty-minute capture should be able to see that it is going to the drive
they meant without leaving the application.
