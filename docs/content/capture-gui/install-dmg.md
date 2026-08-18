# macOS — DMG

The macOS package is a disk image containing the application bundle, with Qt, libFLAC and
libusb bundled inside it.

Built for Apple Silicon (arm64). Intel Macs are not covered by a released package; build
[from source](../development/building-locally.md) instead.

## Install

Download `DomesdayDuplicator-<version>-macos-arm64.dmg` from the
[releases page](https://github.com/simoninns/DomesdayDuplicator/releases), then:

```bash
# Verify the download first
shasum -a 256 -c SHA256SUMS --ignore-missing
```

Open the `.dmg` and drag **Domesday Duplicator** to the Applications folder.

## First launch: Gatekeeper

**The application is not code-signed or notarised.** Signing requires a paid Apple Developer
ID held by an individual, and that is not something a community project's CI can hold. The
consequence is that a normal double-click gets you:

> "Domesday Duplicator" cannot be opened because the developer cannot be verified.

To open it the first time:

1. Find the application in Finder (`/Applications`).
2. **Right-click** (or Control-click) it and choose **Open**.
3. Click **Open** in the dialog that appears.

macOS remembers the decision, so subsequent launches are ordinary double-clicks.

If macOS refuses outright — on some versions the right-click route has been removed — clear
the quarantine attribute instead:

```bash
xattr -dr com.apple.quarantine "/Applications/Domesday Duplicator.app"
```

Do that only for a download you have verified against `SHA256SUMS`.

## USB access

No driver or permission setup is needed. macOS lets an unprivileged application claim the
device through libusb, so it should be found as soon as it is plugged in.

## Update

Drag the new version over the old one in `/Applications`.

## Uninstall

Move `/Applications/Domesday Duplicator.app` to the Trash. Settings are stored under
`~/Library/Preferences/com.domesday86.ddd-gui.plist` and can be removed separately.

## Next

The [Quick start](quick-start.md) takes it from here: finding the device, setting the
front-end gain, and taking a first capture.
