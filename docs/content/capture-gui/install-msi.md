# Windows — MSI

The Windows package is a standard MSI installer carrying the application, the Qt runtime,
libFLAC and libusb.

**Windows 11 (64-bit) only.** Windows 10 and earlier — including Windows 8, 7 and XP — are
not supported, and no package is built for them. The application is built and tested against
Windows 11 alone; an older release may install, but nothing about it is tested and problems
on it are not something the project can act on.

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
anything on the USB bus until WinUSB is bound to it, and shipping a driver package requires
a signed `.inf` — the same problem as above.

Windows binds drivers **by USB identifier**, so this is not one binding: it is one per
identifier the hardware can present, and a board that is new, legacy or half-programmed
presents *different* identifiers from a working one. That is why a board can be plugged in,
lit up and completely absent from the application. The bindings are done once per machine
with [Zadig](https://zadig.akeo.ie/), and they persist.

| What it is | Shows in Zadig as | USB ID | Bind it when |
| --- | --- | --- | --- |
| A working Duplicator | `Domesday Duplicator (…)` | `1209:2347` | Always — this is what the capture window opens. Already done if this machine has ever run one |
| The FX3 in its boot ROM | `WestBridge` | `04B4:00F3` | The board is new, legacy, or an update was interrupted |
| The DE0-Nano's on-board USB-Blaster | `USB-Blaster` | `09FB:6001` | You are bringing the board up, or reprogramming its FPGA |

Binding any of them is the same four steps:

1. Plug the device in.
2. Run Zadig, and choose **Options → List All Devices**.
3. Pick the device by the identifiers in the table — the **USB ID** field in Zadig shows
   them, and it is the field to go by rather than the name.
4. Choose **WinUSB** in the driver box and click **Replace Driver**.

The application says so plainly when it cannot open a device it can see, so a missing
binding looks like a clear message rather than an empty device list.

## If the board does not show up as a Domesday Duplicator

If Zadig lists no `1209:2347` at all, your board is not running this project's firmware, and
no amount of driver binding will make it appear as a Duplicator. What is listed instead says
which case you are in:

| Zadig lists | What the board is | What it needs |
| --- | --- | --- |
| `04B4:00F3` (`WestBridge`) | An FX3 sitting in its boot ROM: a newly built kit whose EEPROM has never been written, or a board left half-programmed by an interrupted update | Bring-up, or a firmware repair |
| `1D50:603B` | The **original** Duplicator firmware, from before this application existed | Bring-up |
| `04B4:0007` only | The FX3 kit's debug serial port. The board has power but its USB 3.0 link is not answering | Check the USB 3.0 cable, and that it is in a USB 3.0 socket |
| Nothing from that board at all | No data cable, no power | A cable that carries **data**, in a working socket |

For the first two, the way forward is
**Tools ▸ Firmware ▸ Bring up a new or legacy board…** — see
[Bringing up a new or legacy board](bringing-up-a-board.md). It programs the FX3's firmware
*and* both images in the FPGA's flash, which is what a new or legacy board needs; the
firmware alone would leave you with a device that enumerates and cannot capture.

**On Windows that wizard needs up to three bindings**, and each has to be in place before
the step that uses it:

1. **`04B4:00F3` — the FX3 in its boot ROM.** The wizard fits jumper J4 and hands the
   firmware to the FX3's built-in loader over the USB 3.0 cable. Until WinUSB is bound to
   `04B4:00F3` the application cannot open the board in that state at all, so the wizard's
   connectivity page marks the FX3 row as a problem and will not go on.

   If the board is not in its boot ROM yet, you can put it there to do the binding: fit
   jumper J4 across the FX3 board's two-pin `PMODE` header, unplug **both** cables, and plug
   them back in. It will enumerate as `04B4:00F3`. That is also the first thing the wizard
   asks for, so nothing is wasted.

2. **`09FB:6001` — the DE0-Nano's on-board USB-Blaster.** This is the JTAG cable that loads
   the gateware into the FPGA, and it is the only route to a board whose flash holds nothing
   the application can talk to. It appears on the DE0-Nano's **mini-USB** connector, which is
   a second cable to the same assembly — connect it before running Zadig. Without this
   binding the wizard's FPGA row is a problem too.

   **An unbound device is invisible to the application**, not merely unopenable, so the
   wizard can report *no USB-Blaster is attached* about a cable that is plainly plugged in
   and listed by Zadig. On Windows, read that as *not bound* before suspecting the cable.

   A **charge-only mini-USB cable** is the commonest thing that goes wrong here: the board
   lights up either way, and nothing enumerates. If Zadig lists no `09FB` device, try
   another cable before anything else.

   The application drives the original USB-Blaster, `09FB:6001`, which is what the DE0-Nano
   carries on board. A USB-Blaster II (`09FB:6010` or `09FB:6810`) is a different cable and
   is not driven — the application names it rather than saying nothing was found.

   If you have **Quartus** installed, its own Altera USB-Blaster driver may already be bound
   to this device. Replacing it with WinUSB takes the cable away from Quartus; undo it in
   Device Manager if you want Quartus to have it back. Only one of them can hold the cable.

3. **`1209:2347` — what the board becomes.** The wizard does not merely program the FX3 and
   stop: step 6 hands the firmware to the boot ROM, the board restarts into it, and the
   wizard then **reopens it as a Duplicator** to write the EEPROM and both gateware images.
   So `1209:2347` is needed part way through the wizard, not after it.

   **Usually there is nothing to do here.** If this machine has ever had a working Duplicator
   plugged into it — this board before it was reprogrammed, or another unit entirely — the
   binding is already there and persists, and the wizard runs straight through.

   It only bites on a machine that has **never** seen a working Duplicator, and there it
   cannot be done in advance: nothing presents `1209:2347` until step 6 has programmed the
   FX3, and Zadig can only bind a device it can see. On such a machine, the first bring-up
   goes:

   1. Bind `04B4:00F3` and `09FB:6001`, and start the wizard as normal.
   2. Step 6 sends the firmware and the board restarts into it, appearing as `1209:2347` for
      the first time. Windows has no driver for that identifier, so the wizard cannot open
      it, and after thirty seconds it reports that **the device did not come back after
      being given its firmware**.
   3. **Leave everything plugged in.** Nothing was written permanently — the failure is
      before the first write, with the firmware running out of memory — and the board is on
      the bus now, which is what Zadig needs. Bind `1209:2347` to WinUSB.
   4. Run the wizard again from the start. It assumes nothing about the board's state and
      running it twice is harmless; this time it goes through.

   **You do not need to quit the application to do this.** Its device list is re-read five
   times a second, so a binding made while it is running is picked up as soon as Windows
   re-enumerates the device. Only the wizard is restarted, not the program — and if you have
   Zadig already open on **Options → List All Devices**, binding it inside the thirty seconds
   step 6 waits lets that step carry on by itself.

The bindings survive the bring-up, so a board that later needs bringing up again — or a
second board on the same machine — needs none of this repeated.

## Update

Run the newer MSI. It upgrades in place — there is no need to uninstall first.

## Uninstall

**Settings → Apps → Installed apps → Domesday Duplicator → Uninstall**, or Add/Remove
Programs. The WinUSB driver bindings are separate and stay; undo them in Device Manager if
you want a device back on its original driver.

## First time through

In this order:

1. **Bind the device to WinUSB** with Zadig, as above. Until that is done the application
   cannot open it.

2. **If the board has never been programmed, or is running the original firmware**, bind
   `04B4:00F3` and `09FB:6001` as well, then run
   **Tools ▸ Firmware ▸ Bring up a new or legacy board…** — see
   [If the board does not show up as a Domesday Duplicator](#if-the-board-does-not-show-up-as-a-domesday-duplicator)
   above, and [Bringing up a new or legacy board](bringing-up-a-board.md). A board that
   already captures needs none of this.

3. **Tell the application what SW401 is set to.** **File → Settings…**, and set **Front-end
   gain** to the position of the four-way DIP switch on the Duplicator board. The switch is
   mechanical and has no electrical path to anything the application can read, so until it is
   declared every level is shown in converter codes rather than in millivolts. Nothing about
   the capture itself depends on it — see [Front-end gain](settings.md#front-end-gain).

4. **Take a first capture.** The [Quick start](quick-start.md) walks through monitoring,
   setting the player's RF output by what is on screen, and writing a file.
