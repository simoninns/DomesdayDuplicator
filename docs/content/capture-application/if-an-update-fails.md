# If an update fails

**Your Duplicator is not damaged.** That is the first thing to say, and it is true of every way an update can go wrong: a pulled cable, a power cut, a closed laptop, a machine that crashed part way through.

This page is what to do about it, and it is also the page to read if you have just built a Duplicator and are wondering why it does not appear as one.

## Recovery mode

If the update did not finish, or if the Duplicator has never had its firmware installed, it starts up in **recovery mode**.

You will see it named that way in three places: the status bar says *Device attached with no firmware*, the **Device** list in the Capture panel says *recovery mode, no firmware installed*, and **Help → Firmware…** opens on a message that says the same thing.

Recovery mode is not a fault. It is the state the USB chip falls back to when it cannot find software it is willing to run, and it exists precisely so that a half-written update leaves you somewhere you can start again from. The chip refuses to run anything it has not fully checked, so it is never running half an update.

### Repairing it

1. Plug the Duplicator in, on its own if you have more than one.
2. Open **Help → Firmware…**.
3. Press **Choose update file…** and pick a `.dddfw` update file — the same kind of file an ordinary update uses. If you still have the one you were installing, use that.
4. Press **Program this device**.

That is the whole procedure. It is the same file, the same window and the same checks as a normal update; the only difference is that it starts by handing the Duplicator the firmware to run, instead of asking firmware that is already running to update itself.

It takes a minute or two, and — as always — **leave the device plugged in and powered** while it runs.

### Why the button says "program" and not "repair"

Because the Duplicator cannot tell the difference, and neither can the application.

A chip that has never been programmed and a chip whose update was interrupted look exactly the same over the cable: in both cases the chip finds nothing it will run and waits for a host. So the window offers one action that is correct for both. If you have just built a board, nothing has gone wrong and there is nothing to repair — this is simply how a new board is set up.

## A brand-new Duplicator

A Duplicator you have just built arrives in recovery mode, and the steps above are how you bring it to life. You need the USB cable and a release update file; no programming tools, no jumpers, and nothing to install.

The FPGA is a separate matter. A board whose FPGA has never been programmed still needs its gateware written once over the DE0-Nano's own USB connector, which is covered in the [hardware programming](../development/hardware-programming/index.md) pages.

## Two states that look alike but are not

**"Running a programming tool."** If the Firmware window says this, something left the Duplicator holding a Cypress loader in its memory — usually a `fx3-programmer` session that did not finish. Unplug it and plug it back in. It will come back in recovery mode, and the steps above then apply.

**"This firmware requires a newer application."** Nothing has failed. The device has been updated by a newer version of the application than the one you are running. Update the application, and it will work again.

## On Windows: the driver

Windows binds a driver to a device by its USB identifiers, and a Duplicator in recovery mode reports *different identifiers* from a working one — it is, as far as Windows is concerned, a different device. So the driver that was bound to your working Duplicator is not bound to the same unit in recovery mode, and until one is, the application cannot open it.

If the Firmware window does not list your device on Windows even though it is plugged in and its light is on, this is why. Bind WinUSB to it:

1. Download [Zadig](https://zadig.akeo.ie/).
2. With the Duplicator plugged in, run Zadig and choose **Options → List All Devices**.
3. Pick the device shown as `WestBridge` with the identifiers `04B4` and `00F3`.
4. Choose **WinUSB** as the driver and press **Install Driver**.

That is a once-per-machine step, and it is the same thing Windows does automatically for a working Duplicator. Linux and macOS need nothing: on Linux the project's udev rules already cover the recovery identifiers, and macOS binds nothing at all.

## If none of this reaches it

Everything above needs the USB chip's own built-in loader to be answering. If it is not — no device appears at all, on any machine, with a cable known to work — then the problem is not software, and the bench procedures in the [hardware programming](../development/hardware-programming/index.md) pages are the next place to look.

That is a genuinely rare case. Nothing an update does can put a Duplicator there: the loader is in read-only memory inside the chip and cannot be overwritten by anything this application sends.

## What the application does behind the scenes

Enough to satisfy the curious, in one paragraph.

A Duplicator in recovery mode is running the USB chip's built-in loader, which understands one command: *put these bytes at this address*. The application hands it the firmware out of the update file you chose, tells it to run that, and waits for the Duplicator to reappear — now running its proper firmware, out of memory rather than out of its own storage. From that point it is an ordinary update: the firmware writes the update file into the Duplicator's storage, reads all of it back, checks it against the file, and only then restarts. Nothing about programming a new board takes a shortcut around the checks a routine update makes. The full description is on the [Device update mechanism](../development/device-update-mechanism.md) page.
