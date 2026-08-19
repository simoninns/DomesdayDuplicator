# Windows — MSI

The Windows package is a standard MSI installer carrying the application, the Qt runtime,
libFLAC and libusb.

## Install

Download `DomesdayDuplicator-<version>-windows-x64.msi` from the
[releases page](https://github.com/simoninns/DomesdayDuplicator/releases), then verify it
in PowerShell:

```powershell
Get-FileHash DomesdayDuplicator-<version>-windows-x64.msi -Algorithm SHA256
```

and compare against the line for that file in `SHA256SUMS`.

Run the installer. It installs to `C:\Program Files\Domesday Duplicator` and adds a Start
menu shortcut.

## First launch: SmartScreen

**The installer is not code-signed**, so Windows will show:

> Windows protected your PC — Microsoft Defender SmartScreen prevented an unrecognised app
> from starting.

Click **More info**, then **Run anyway**. Code signing needs a certificate held by an
individual, which is not something a community project's CI can hold; verifying the
download against `SHA256SUMS` gives you the same assurance that the file is the one that
was released.

## You still need the WinUSB driver

**The MSI does not install a device driver.** Windows will not let the application talk to
the Domesday Duplicator until WinUSB is bound to it, and shipping a driver package requires
a signed `.inf` — the same problem as above.

Use [Zadig](https://zadig.akeo.ie/) once, per machine:

1. Plug in the Domesday Duplicator.
2. Run Zadig, and choose **Options → List All Devices**.
3. Select the Domesday Duplicator (USB ID `1209:2347`).
4. Choose **WinUSB** as the driver and click **Replace Driver**.

If the device shows as `04B4:00F3` instead, the FX3 has no firmware loaded and is sitting in
its bootloader — see [FX3 firmware](../development/hardware-programming/fx3-firmware.md).

The application says so plainly when it cannot open the device, so a missing driver looks
like a clear message rather than an empty device list.

## Update

Run the newer MSI. It upgrades in place — there is no need to uninstall first.

## Uninstall

**Settings → Apps → Installed apps → Domesday Duplicator → Uninstall**, or Add/Remove
Programs. The WinUSB driver binding is separate and stays; undo it in Device Manager if you
want the device back on its original driver.

## First time through

In this order:

1. **Bind the device to WinUSB** with Zadig, as above. Until that is done the application
   cannot open it.

2. **Tell the application what SW401 is set to.** **File → Settings…**, and set **Front-end
   gain** to the position of the four-way DIP switch on the Duplicator board. The switch is
   mechanical and has no electrical path to anything the application can read, so until it is
   declared every level is shown in converter codes rather than in millivolts. Nothing about
   the capture itself depends on it — see [Front-end gain](settings.md#front-end-gain).

3. **Take a first capture.** The [Quick start](quick-start.md) walks through monitoring,
   setting the player's RF output by what is on screen, and writing a file.
