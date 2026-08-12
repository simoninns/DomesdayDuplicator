# Linux device access

On Linux a USB device node is created owned by `root`, readable by everyone and writable by
nobody else. `fx3-programmer` and the capture application both need **write** access, so
without a udev rule every operation fails — and it fails in a way that looks like the device
is missing rather than forbidden:

```
$ fx3-programmer -l
No FX3 devices found
```

even though `lsusb` clearly shows it. If you see that, this page is the fix.

## What the rules have to cover

A Domesday Duplicator presents **four different USB identities** depending on what it is
doing, and a rule matching only one of them will appear to work until the moment it does not:

| Identity | When | Needed by |
| --- | --- | --- |
| `04b4:00f3` | FX3 in bootloader mode, PMODE jumper fitted | `fx3-programmer` |
| `04b4:4720` | Transient, while the EEPROM is being written | `fx3-programmer` |
| `1d50:603b` | Running Domesday Duplicator firmware | The capture application |
| `09fb:6001` | The DE0-NANO's onboard USB-Blaster, whenever it is powered | Quartus (`quartus_pgm`, `jtagconfig`) |

The first three are the FX3 and are covered by `70-domesday-duplicator.rules`: everything with
the Cypress vendor ID `04b4`, plus `1d50:603b`.

The fourth is the FPGA's JTAG cable and is covered by a separate file,
`70-altera-usb-blaster.rules`, which also handles the other USB-Blaster and USB-Blaster II
product IDs (`6002`, `6003`, `6010`, `6810`) for anyone using a standalone cable.

!!! note "Quartus does not install this rule for you"

    Neither Altera's own installer nor the nixpkgs package ships any udev rules. Without one
    the JTAG device is root-only and every Quartus programming operation fails.

## NixOS

The repository provides a NixOS module. Add the flake as an input and enable it:

```nix
{
  inputs.domesdayduplicator.url = "github:simoninns/DomesdayDuplicator";

  # ... in your configuration:
  imports = [ domesdayduplicator.nixosModules.udev ];
  hardware.domesdayDuplicator.enable = true;
}
```

Then `sudo nixos-rebuild switch`. That one option covers all four identities — the FX3 rules
and the USB-Blaster rules together — and by default also installs `fx3-programmer` itself.
Two options narrow it:

| Option | Effect |
| --- | --- |
| `hardware.domesdayDuplicator.installProgrammer = false` | Permissions only, no `fx3-programmer` in `systemPackages` |
| `hardware.domesdayDuplicator.usbBlaster = false` | Skip the USB-Blaster rules, on a machine that captures but never reprogrammes the FPGA |

## Debian, Ubuntu, Fedora, Arch and others

Install both rules files from the repository:

```bash
sudo install -m 644 fx3/programmer/configs/70-domesday-duplicator.rules \
     /etc/udev/rules.d/70-domesday-duplicator.rules
sudo install -m 644 fpga/configs/70-altera-usb-blaster.rules \
     /etc/udev/rules.d/70-altera-usb-blaster.rules
sudo udevadm control --reload
sudo udevadm trigger --action=add --subsystem-match=usb
```

If you built and installed `fx3-programmer` with `cmake --install`, the first file is already
at `<prefix>/lib/udev/rules.d/70-domesday-duplicator.rules` and can be symlinked instead of
copied.

!!! warning "Remove any older Altera rules file first"

    Older versions of this project's documentation, and many Altera installation guides,
    suggest creating `/etc/udev/rules.d/40-altera-usbblaster.rules` by hand. If you have one,
    delete it. It grants access through `MODE="0666"` alone, with no `uaccess` tag, so it
    hands write access to *every* user and process on the machine rather than to the user at
    the console — and having two files matching the same device makes it much harder to work
    out which one is responsible when something is wrong.

## The filename matters

!!! warning "Do not rename the rules file to something sorting after `73`"

    `udev` processes rule files in **lexical order**, and systemd consumes the `uaccess` tag
    in `73-seat-late.rules`:

    ```
    TAG=="uaccess", ENV{MAJOR}!="", RUN{builtin}+="uaccess"
    ```

    A rule that sets the tag in a file sorting *after* 73 sets it after anything looks for
    it. No ACL is applied and the tag is silently ignored.

    This project's rules were called `88-cyusb.rules` until 2026 and had exactly that bug —
    the `uaccess` half had never once worked on any machine. It went unnoticed because the
    rules also set `MODE="0666"`, which *is* applied regardless of ordering, so the file
    appeared to do its job.

    The `70-` prefix is load-bearing. Keep it.

## Confirming it worked

Plug the device in — or re-trigger udev as above — and look at the node.

Find it first:

```bash
$ lsusb | grep -iE "04b4|1d50"
Bus 008 Device 005: ID 1d50:603b OpenMoko, Inc. Raspiface
```

`OpenMoko, Inc. Raspiface` is not a mistake: `1d50:603b` is an ID from the
[OpenMoko free USB ID pool](http://wiki.openmoko.org/wiki/USB_Product_IDs), and `usb.ids`
labels it with a different project that shares the pool. The `iProduct` string is what
identifies the device — see [FX3 firmware](fx3-firmware.md#confirming-what-is-running).

Then check the permissions on that bus and device number:

```bash
$ ls -la /dev/bus/usb/008/005
crw-rw-rw-+ 1 root root 189, 900 Aug 12 20:36 /dev/bus/usb/008/005
```

The trailing **`+`** is the important character. It means an ACL is present. Confirm it names
you:

```bash
$ getfacl -p /dev/bus/usb/008/005
user::rw-
user:sdi:rw-        <-- you, granted by uaccess
group::rw-
mask::rw-
other::rw-
```

Finally, the tool that matters:

```bash
$ fx3-programmer -l
Found 1 FX3 device(s):

[0] VID:PID=1d50:603b Bus=008 Device=005 Mode=Application (Domesday Duplicator)
```

### And for the USB-Blaster

Same idea, different tool. `jtagconfig` scans the JTAG chain, which needs write access:

```bash
$ jtagconfig
1) USB-Blaster [7-3.2]
  020F30DD   10CL025(Y|Z)/EP3C25/EP4CE22
```

The second line is the FPGA it found, read back over JTAG — so this confirms both the
permissions and the cable's connection to the board.

Without the rule, the same command reports:

```
1) USB-Blaster variant [7-3.2]
  Unable to lock chain - Insufficient port permissions
```

Note that it says **`USB-Blaster variant`** rather than `USB-Blaster`. Quartus cannot open the
device far enough to identify which cable it is, so it falls back to a generic name. That
wording is itself a permissions symptom, and it is easy to mistake for a hardware problem.

## If it still does not work

| Symptom | Cause |
| --- | --- |
| No `+` on the device node | The rule did not match, or was not reloaded. Re-run `udevadm control --reload` **and** `udevadm trigger`, or simply replug the device — rules are applied at enumeration, so a device plugged in before the rule was installed keeps its old permissions |
| `+` present but no `user:` line for you | `uaccess` grants to the user at the **active local seat**. Over SSH, or as a second logged-in user, you will not get it. Fall back on the `MODE="0666"` the same rules set, or run as root |
| Rule present, no ACL, no error | Check the filename sorts before `73-seat-late.rules` — see above |
| Works for `1d50:603b` but not for programming | Your rule matches only the application identity. The bootloader is `04b4:*` |
| FX3 works but Quartus reports `Insufficient port permissions` | The USB-Blaster is a separate device with a separate rules file. Install `70-altera-usb-blaster.rules` too |
| `No JTAG hardware available` | Not a permissions problem — the blaster is not enumerating at all. Check `lsusb` shows `09fb:6001`, and that the DE0-NANO's mini-USB cable carries data rather than power only |
