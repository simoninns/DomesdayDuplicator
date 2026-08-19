# macOS — DMG

The macOS package is a disk image containing the application bundle, with Qt, libFLAC and
libusb bundled inside it.

**Apple Silicon (arm64) only.** Intel Macs are not supported, and no package is built for
them. The application is built and tested against Apple Silicon alone; on an Intel Mac you
would have to build [from source](../development/building-locally.md), and nothing about
that path is tested.

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

## First time through

In this order:

1. **Plug the Duplicator in.** No driver or permission setup is needed on macOS.

2. **Tell the application what SW401 is set to.** **File → Settings…**, and set **Front-end
   gain** to the position of the four-way DIP switch on the Duplicator board. The switch is
   mechanical and has no electrical path to anything the application can read, so until it is
   declared every level is shown in converter codes rather than in millivolts. Nothing about
   the capture itself depends on it — see [Front-end gain](settings.md#front-end-gain).

3. **Take a first capture.** The [Quick start](quick-start.md) walks through monitoring,
   setting the player's RF output by what is on screen, and writing a file.
