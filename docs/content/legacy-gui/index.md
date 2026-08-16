# Legacy Capture Application

!!! warning "This is not the application you install"

    The released packages — Flatpak, DMG and MSI — carry the current
    [capture application](../capture-gui/index.md). This section documents the older
    application it replaces, which is still in the repository under `gui/` but is no longer
    built, tested or packaged by CI.

The older application is kept as a reference, not as a supported install. It is the one that
made every capture up to the changeover, and until its replacement has passed the hardware
capture-integrity procedure it stays in the tree so that a capture can be repeated with the
software that made the original.

## What is only here

Two things the older application does that the current one does not yet:

- **LaserDisc player control.** Serial control of the Pioneer LD-V4300D and CLD-V2800, and
  the automatic whole-disc capture built on it.
- **Advanced capture naming and the metadata sidecar.**

If you need either, this is the application that has them.

## What moved

| If you are looking for | Go to |
| --- | --- |
| Installing the application | [Capture Application](../capture-gui/index.md) |
| Updating the device's firmware or gateware | [Updating your Domesday Duplicator](../capture-gui/updating-your-domesday-duplicator.md) |
| What a capture file is, and what reads it | [Capture files](../capture-gui/capture-files.md) |

## Pages in this section

- [User Guide for Linux](user-guide.md) — the original guide, unchanged
- [Capture formats](capture-formats.md) — the `.ldf` and `.lds` formats this application
  writes and reads
- [Building from source](building-from-source.md) — building `gui/`
