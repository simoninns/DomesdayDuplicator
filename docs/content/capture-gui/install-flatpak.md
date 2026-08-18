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

## Next

The [Quick start](quick-start.md) takes it from here: finding the device, setting the
front-end gain, and taking a first capture.
