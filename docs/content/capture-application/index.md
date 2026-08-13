# Capture Application

The capture application drives the Domesday Duplicator board: one-click RF capture, live
signal amplitude monitoring, capture metadata, and serial control of Pioneer LD-V4300D and
CLD-V2800 players for automatic whole-disc capture.

## Which package do I want?

| Platform | Package | Notes |
| --- | --- | --- |
| Linux | [Flatpak](install-flatpak.md) | Also needs udev rules installed on the host — a Flatpak cannot do that for you |
| macOS | [DMG](install-dmg.md) | Unsigned, so the first launch needs a Gatekeeper step |
| Windows | [MSI](install-msi.md) | The device also needs the WinUSB driver bound with Zadig |

Every release also carries a `SHA256SUMS` file so a download can be verified, and a
`PROVENANCE.txt` recording the commit and the toolchains it was built with.

Building it yourself — for development, or for a platform the packages do not cover — is
described in [Building from source](building-from-source.md).

## What it writes

Captures are written as **FLAC**, in a `.ldf` file. This is the same format the ld-decode
and vhs-decode toolchains read, so a capture goes straight into a decode with no conversion
step in between, at roughly half the size of the raw sample data.

An uncompressed 16-bit format (`.raw`) is also available for a machine that cannot keep up
with the encoder. See [Capture formats](capture-formats.md) for what changed, why, and how
to convert to the older `.lds` format if something in your workflow still needs it.

## Releases

The capture application is released on its own schedule, under **`gui-v*`** tags. The
firmware and gateware release separately under `fw-v*` tags — a fix to the application does
not mean you need to reflash anything.

[Releases on GitHub](https://github.com/simoninns/DomesdayDuplicator/releases)
