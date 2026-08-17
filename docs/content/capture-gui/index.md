# Capture Application

The capture application drives the Domesday Duplicator board. It finds the device, streams
40 million samples a second off it, proves that nothing was lost on the way, shows you what
the signal is doing while it happens, and writes it to disk in a format the decode
toolchains open directly. It also installs the software that runs inside the Duplicator
itself.

If you have just plugged a Duplicator in for the first time, go to the
**[Quick start](quick-start.md)**.

## Which package do I want?

| Platform | Package | Notes |
| --- | --- | --- |
| Linux | [Flatpak](install-flatpak.md) | Also needs udev rules installed on the host — a Flatpak cannot do that for you |
| macOS | [DMG](install-dmg.md) | Unsigned, so the first launch needs a Gatekeeper step |
| Windows | [MSI](install-msi.md) | The device also needs the WinUSB driver bound with Zadig |

Every release also carries a `SHA256SUMS` file so a download can be verified, and a
`PROVENANCE.txt` recording the commit and the toolchains it was built with.

Building it yourself — for development, or for a platform the packages do not cover — is
described in [Building locally](../development/building-locally.md).

## The parts of it

The application is a set of panels around one stream. You can float them, close them and
rearrange them; [The main window](main-window.md) covers that, and the pages below cover
what each one is for.

| | What it does |
| --- | --- |
| [Capture control](capture-control.md) | Choosing the device, where the file goes, and the two buttons that start things |
| [Statistics](statistics.md) | Throughput, integrity, buffer depth and clipping, second by second |
| [Signal analysis](signal-analysis.md) | The scope, the spectrum and spectrogram, and five minutes of level history |
| [Capture files](capture-files.md) | What gets written, what it is called, and what reads it |
| [Naming and metadata](capture-naming.md) | Saying what the disc is, and the metadata file written beside every capture |
| [Settings](settings.md) | Buffer queue, USB transfer mode, preferred device and the front-end gain declaration |
| [Player control](player-control.md) | Driving a LaserDisc player over its serial port: examining a disc, and capturing a side by itself |
| [Test mode](test-mode.md) | Proving the capture path with the gateware's test pattern |
| [Updating your Duplicator](updating-your-domesday-duplicator.md) | Installing firmware and gateware into the device |
| [Command line](command-line.md) | The options the application accepts, and the scriptable analysis |

## Monitoring and capturing are different things

This is the one idea worth knowing before anything else.

**Monitoring** opens the device and runs the stream with nothing on the end of it. The
signal is validated and measured, every display is live, and nothing is written anywhere.
It is what you use to set a player's RF output, to check a cable, or to see whether a disc
is worth capturing.

**Capturing** is the same stream with a file attached to it. Starting a capture from idle
starts the stream too, so the common case is a single press; stopping a capture leaves the
stream running, so the next side of a disc is one press away and the device is never
reopened between them.

## What it writes

Captures are written as native **FLAC**, with the compound extension `.ddd.flac` — a plain
`.flac` to anything that reads audio, with the `.ddd` in front of it saying where the
samples came from. ld-decode and vhs-decode take one directly, at roughly half the size of
the raw sample data.

Each file carries its own provenance in its tags: the build that made it, the real 40 MHz
sample rate that a FLAC header cannot express, whether it was taken in test mode, and the
front-end gain if one was declared. See [Capture files](capture-files.md).

## Releases

The capture application is released on its own schedule, under **`gui-v*`** tags. The
firmware and gateware release separately under `fw-v*` tags — a fix to the application does
not mean you need to reflash anything, and a firmware release does not mean you need a new
application.

[Releases on GitHub](https://github.com/simoninns/DomesdayDuplicator/releases)
