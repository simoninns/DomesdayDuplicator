# Quick start

From a Duplicator in a box to a capture on disk. Fifteen minutes, most of it waiting for a
download.

You need the Duplicator, a USB 3 cable, a computer with a USB 3 port, and somewhere to put
the file — a capture writes about 40 MB every second, so roughly 145 GB an hour.

## 1. Install the application

Take the package for your platform and follow the page for it, including the part about
device access. That step is the one that catches people out, and it is different on each
platform:

- **Linux** — [Flatpak](install-flatpak.md), plus **udev rules on the host**
- **macOS** — [DMG](install-dmg.md); nothing else to do, macOS lets the application claim
  the device on its own
- **Windows** — [MSI](install-msi.md), plus **binding the WinUSB driver with Zadig**

## 2. Plug the Duplicator in

Use a **USB 3** port, directly on the computer rather than through a hub. The device
produces 80 MB/s and a USB 2 port physically cannot carry it.

Start the application. Look at the **Capture** panel:

| What it says | What it means |
| --- | --- |
| A path in the **Device** list, and *Ready* below | Found, and it can capture |
| *connected at insufficient speed* | It is on a USB 2 port. Move it |
| *recovery mode, no firmware installed* | The device needs programming — go to [Updating your Duplicator](updating-your-domesday-duplicator.md), which does first-time programming too |
| *No capture device attached* | Nothing was found. On Linux that is almost always the udev rules; on Windows, the WinUSB binding |

## 3. Tell it what the gain switch is set to

**File → Settings…**, and set **Front-end gain** to the position of **SW401** on the
Duplicator board.

The switch is mechanical and has no connection to anything the application can read, so it
has to be told. Until it is, every level is shown in converter codes rather than
millivolts — which is correct rather than unhelpful: a guess here would put
authoritative-looking voltages on screen that could be wrong by a factor of four.

The switches are written the way they sit on the board, left to right: `1010` is switches 1
and 3 closed and the other two open. If you are not sure, leave it undeclared and work in
codes; nothing about the capture itself depends on it. See
[Settings](settings.md#front-end-gain).

## 4. Start monitoring

Press **Start monitoring**. Nothing is written yet — this is the stream with no file on the
end of it.

Now play a disc and look at three things:

1. **Statistics ▸ Throughput** should settle at about **76 MB/s (40.00 Msps)**. That figure
   is the converter's rate, and a working device cannot exceed it.
2. **Statistics ▸ Integrity** should say **Verified — no samples lost**.
3. **Amplitude History**, or **Signal level** in the Statistics panel, should show the
   signal filling a good part of the range without reaching the ends. The
   [amplitude strip](signal-analysis.md#amplitude-history) marks the recommended 75 % bounds
   on both sides — aim to sit inside them, and check **Clipping** stays at zero.

Adjust the player's RF output, or the SW401 gain, until it does. This is what monitoring is
for, and it costs nothing: you can leave it running for as long as you like.

## 5. Choose where the capture goes

Still in the **Capture** panel:

- **Folder** — `Browse…` to the drive you want. The **Free space** row answers the question
  you actually have: not "how many gigabytes", but *how much capture this volume holds*.
- **Name** — leave it empty and each capture is named after the time it was taken, which is
  what keeps a folder of them in order. Type one and it is used instead.
- **Compression** — leave it at 8 unless the machine cannot keep up.

## 6. Capture

Press **Start capture**.

The button turns red — "recording", not "fault" — the status bar names the file being
written, and the Statistics
panel gains **Written** and **Encoder backlog** rows. Everything else carries on as it was —
a capture is the monitoring stream with a file attached, so the displays do not change.

Press **Stop capture** when the side ends. The stream keeps running, so the second side is
one more press: turn the disc over, press **Start capture** again.

While it runs, the two figures worth watching are **Buffer queue** and **Encoder backlog**.
Both should sit near zero. If either climbs and stays up, the machine is not keeping up —
[Statistics](statistics.md) says which of the two to act on and
[If a capture fails](if-a-capture-fails.md) says what to do about it.

!!! tip "If your player has a serial port"

    That is the capture taken by hand, and it is the path to know first. Turn on
    [player control](player-control.md) and the application will do the whole of it for you
    instead: **Tools ▸ Automatic capture…** examines the disc, names the capture from what it
    found, plays the side, and stops both the capture and the player at the end.

## 7. Use the file

You now have a `.ddd.flac` in the folder you chose. It goes straight into a decode:

```bash
ld-decode capture.ddd.flac output
```

Nothing to convert, and nothing to remember about it later — the sample rate, the build that
made it and the declared gain travel inside the file. See [Capture files](capture-files.md).

## If something is wrong

- The device is not found, or the capture stops — [If a capture fails](if-a-capture-fails.md)
- You want to prove the capture path itself rather than a disc — [Test mode](test-mode.md)
- The device does not appear as a Duplicator at all — [If an update fails](if-an-update-fails.md),
  which is also the page for a board that has never been programmed
