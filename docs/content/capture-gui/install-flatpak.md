# Linux — Flatpak

The Linux package is a Flatpak bundle, which brings its own Qt and libFLAC and so does not
care which distribution you run it on.

## Install

Download `DomesdayDuplicator-<version>.flatpak` from the
[releases page](https://github.com/simoninns/DomesdayDuplicator/releases), then:

```bash
# Verify the download first
sha256sum -c SHA256SUMS --ignore-missing

# Flathub provides the KDE runtime the application is built against
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo

flatpak install --user DomesdayDuplicator-<version>.flatpak
```

Run it from your desktop's application menu, or:

```bash
flatpak run io.github.simoninns.DddGui
```

## You still need udev rules

**This is the step that catches people out.** A Flatpak cannot install udev rules — they are
host system configuration, and the sandbox has no business writing them. Without them the
device node belongs to root and the application cannot open it, which shows up as the
device simply not being found.

Install the rules once, on the host:

```bash
sudo curl -o /etc/udev/rules.d/70-domesday-duplicator.rules \
  https://raw.githubusercontent.com/simoninns/DomesdayDuplicator/main/fx3/programmer/configs/70-domesday-duplicator.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then unplug and replug the device.

!!! warning "Keep the `70-` filename"

    It sorts before `73-seat-late.rules`, which is the file that acts on the `uaccess` tag
    the rules set. Save it under a name sorting after that and the tag is set with nothing
    left to read it, leaving only the rules' `MODE="0666"` fallback doing any work.

One file is all you need. It covers the capture device, the FX3 in its recovery identities,
the debug UART, **and the FPGA's USB-Blaster** — the JTAG cable used to reprogram a board
that an update has left unresponsive. You almost certainly will not need that last one, but
the day you do you will not want to be diagnosing a permissions failure first, so it is
included here rather than in a separate step you would have skipped.

On NixOS, use the module the repository ships instead:

```nix
{
  imports = [ inputs.domesdayduplicator.nixosModules.udev ];
  hardware.domesdayDuplicator.enable = true;
}
```

Details, including how to check the rules took effect, are in
[Linux device access](../development/hardware-programming/linux-device-access.md).

## Why it asks for network access

It does not use the network. The application makes no network connections at all — it does
not check for updates, report anything, or fetch anything.

The permission is there for one reason: a Flatpak without it is put in a network namespace
of its own, and the kernel does not deliver device-attach events into one. Those events are
how the application learns that a Duplicator has been plugged in. Without the permission it
finds a device that was already attached when it started and never notices one attached
afterwards — and, during an update, never notices the device coming back after it restarts
itself.

If you would rather not grant it, you can take it away:

```bash
flatpak override --user --unshare=network io.github.simoninns.DddGui
```

The application is built to cope. It compares what the kernel says is attached against what
its USB library believes, and where the two differ it restarts its USB connection to catch
up — so a Duplicator plugged in afterwards is still found, a fraction of a second later than
it otherwise would be. The permission is what makes that fallback unnecessary rather than
what makes the application work, and the same fallback is what keeps it working inside a
plain container.

## What player control needs

[Player control](player-control.md) drives a LaserDisc player over a serial cable, and that
is a second cable with a second permission behind it. Three things are worth knowing before
you go looking for a Flatpak setting that does not exist:

- **No override is needed.** The same `--device=all` permission that reaches the Duplicator
  reaches `/dev/ttyUSB*` and `/dev/ttyACM*`, because both are inside `/dev`. There is no
  narrower static permission for a serial port and no portal for one, so this single
  permission covers both cables.
- **Your group membership still applies.** Serial devices belong to a group — `dialout` on
  Debian, Ubuntu and Fedora, `uucp` on Arch and its derivatives — and the sandbox keeps your
  supplementary groups rather than routing around them. `sudo usermod -a -G dialout $USER`,
  then **log out and back in**: a group you have just been granted does not apply to the
  session you are already in.
- **Nothing opens a port until you ask it to.** Player control is off until it is switched
  on, so a Flatpak install touches no serial port at all until then.

The rest — which players are supported, how the port is found, and what to do when nothing
answers — is on the [Player control](player-control.md) page.

## Where captures can be written

The Flatpak is granted your home directory plus `/run/media`, `/media` and `/mnt`, which
covers the usual places an external drive is mounted. Captures are large — an hour of
LaserDisc RF is around 90 to 145 GB — so most people write to a second drive, and those
paths are why that works without further configuration.

If your drive mounts somewhere else, grant it explicitly:

```bash
flatpak override --user --filesystem=/path/to/drive io.github.simoninns.DddGui
```

## Update

```bash
flatpak install --user DomesdayDuplicator-<newer-version>.flatpak
```

## Uninstall

```bash
flatpak uninstall --user io.github.simoninns.DddGui
```

The udev rules are separate and stay behind; remove
`/etc/udev/rules.d/70-domesday-duplicator.rules` if you want them gone too.

## First time through

In this order:

1. **Install the udev rules** on the host, as above. Without them the device is not found at
   all.

2. **Join the serial port's group** — `dialout`, or `uucp` on Arch — if you want [player
   control](player-control.md), and log out and back in. Skip it if you are not using it;
   nothing else needs it.

3. **Tell the application what SW401 is set to.** **File → Settings…**, and set **Front-end
   gain** to the position of the four-way DIP switch on the Duplicator board. The switch is
   mechanical and has no electrical path to anything the application can read, so until it is
   declared every level is shown in converter codes rather than in millivolts. Nothing about
   the capture itself depends on it — see [Front-end gain](settings.md#front-end-gain).

4. **Take a first capture.** The [Quick start](quick-start.md) walks through monitoring,
   setting the player's RF output by what is on screen, and writing a file.
