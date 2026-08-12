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

## What the rule has to cover

The Domesday Duplicator presents **three different USB identities** depending on what it is
doing, and a rule matching only one of them will appear to work until the moment it does not:

| Identity | When | Needed by |
| --- | --- | --- |
| `04b4:00f3` | FX3 in bootloader mode, PMODE jumper fitted | `fx3-programmer` |
| `04b4:4720` | Transient, while the EEPROM is being written | `fx3-programmer` |
| `1d50:603b` | Running Domesday Duplicator firmware | The capture application |

The rules this project ships cover all three: everything with the Cypress vendor ID `04b4`,
plus `1d50:603b`.

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

Then `sudo nixos-rebuild switch`. This installs the rules and, by default, `fx3-programmer`
itself; set `hardware.domesdayDuplicator.installProgrammer = false` if you only want the
permissions.

## Debian, Ubuntu, Fedora, Arch and others

Install the rules file from the repository:

```bash
sudo install -m 644 fx3/programmer/configs/70-domesday-duplicator.rules \
     /etc/udev/rules.d/70-domesday-duplicator.rules
sudo udevadm control --reload
sudo udevadm trigger --action=add --subsystem-match=usb
```

If you built and installed `fx3-programmer` with `cmake --install`, the same file is already
at `<prefix>/lib/udev/rules.d/70-domesday-duplicator.rules` and can be symlinked instead of
copied.

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

## If it still does not work

| Symptom | Cause |
| --- | --- |
| No `+` on the device node | The rule did not match, or was not reloaded. Re-run `udevadm control --reload` **and** `udevadm trigger`, or simply replug the device — rules are applied at enumeration, so a device plugged in before the rule was installed keeps its old permissions |
| `+` present but no `user:` line for you | `uaccess` grants to the user at the **active local seat**. Over SSH, or as a second logged-in user, you will not get it. Fall back on the `MODE="0666"` the same rules set, or run as root |
| Rule present, no ACL, no error | Check the filename sorts before `73-seat-late.rules` — see above |
| Works for `1d50:603b` but not for programming | Your rule matches only the application identity. The bootloader is `04b4:*` |
