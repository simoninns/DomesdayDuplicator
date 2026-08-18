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
sudo curl -o /etc/udev/rules.d/99-domesdayduplicator.rules \
  https://raw.githubusercontent.com/simoninns/DomesdayDuplicator/main/fx3/programmer/configs/70-domesday-duplicator.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Then unplug and replug the device.

On NixOS, use the module the repository ships instead:

```nix
{
  imports = [ inputs.domesdayduplicator.nixosModules.udev ];
  hardware.domesdayDuplicator.enable = true;
}
```

Details, including how to check the rules took effect, are in
[Linux device access](../development/hardware-programming/linux-device-access.md).

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
`/etc/udev/rules.d/99-domesdayduplicator.rules` if you want them gone too.

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
