# If an update fails

**Your Duplicator is not damaged.** That is the first thing to say, and it is true of every way an update can go wrong: a pulled cable, a power cut, a closed laptop, a machine that crashed part way through.

This page is what to do about it, and it is also the page to read if you have just built a Duplicator and are wondering why it does not appear as one.

There are two things inside a Duplicator that an update can replace, and an interrupted update leaves one of two clearly named states behind:

| What you see | What it means | What to do |
| --- | --- | --- |
| The Duplicator does not appear as one at all | **Recovery mode** — the firmware did not finish being written, or has never been written | [Repairing it](#repairing-it), below |
| The Duplicator appears normally but cannot capture | **Recovery gateware** — the gateware did not finish being written | [Reinstalling it](#reinstalling-it), below |

Neither is a fault to diagnose. Both are states the Duplicator falls back to *on purpose*, so that an update that stops part way leaves somewhere to start again from — and in both cases one button in the Firmware window finishes the job.

## Recovery mode

If the update did not finish, or if the Duplicator has never had its firmware installed, it starts up in **recovery mode**.

You will see it named that way in three places: the status bar says *Device attached with no firmware*, the Capture panel's **Status** line says the device has no firmware installed and points at Tools ▸ Firmware ▸ Update firmware…, and **Tools → Firmware → Update firmware…** opens on a message that says the same thing. The **Preferred device** list in **File ▸ Settings…** names it *recovery mode, no firmware installed* as well.

Recovery mode is not a fault. It is the state the USB chip falls back to when it cannot find software it is willing to run, and it exists precisely so that a half-written update leaves you somewhere you can start again from. The chip refuses to run anything it has not fully checked, so it is never running half an update.

### Repairing it

1. Plug the Duplicator in, on its own if you have more than one.
2. Open **Tools → Firmware → Update firmware…**.
3. Press **Choose update file…** and pick a `.dddfw` update file — the same kind of file an ordinary update uses. If you still have the one you were installing, use that.
4. Press **Program this device**.

That is the whole procedure. It is the same file, the same window and the same checks as a normal update; the only difference is that it starts by handing the Duplicator the firmware to run, instead of asking firmware that is already running to update itself.

It takes a minute or two, and — as always — **leave the device plugged in and powered** while it runs.

### Why the button says "program" and not "repair"

Because the Duplicator cannot tell the difference, and neither can the application.

A chip that has never been programmed and a chip whose update was interrupted look exactly the same over the cable: in both cases the chip finds nothing it will run and waits for a host. So the window offers one action that is correct for both. If you have just built a board, nothing has gone wrong and there is nothing to repair — this is simply how a new board is set up.

## Recovery gateware

There are two pieces of software inside a Duplicator, and they fail in different ways. Recovery mode above is the *firmware* — the USB chip's software. **Recovery gateware** is the other half, and it looks quite different from the outside: the Duplicator appears normally, the Firmware window opens, everything answers — and it cannot capture.

You will see it named in the Firmware window: *This device is running its recovery gateware*, with the Gateware row of the version table reading **Recovery gateware** rather than a version.

This is what a Duplicator falls back to when a gateware update does not finish. Its FPGA holds two images: a small resident one that has been there since the board was first set up and is never replaced, and the capture gateware that updates replace. The resident image cannot capture — that is deliberate, and it is the point. It is small enough to be trustworthy, it never changes, and it can always talk to the application well enough to be sent the other half again.

The alternative would have been for the resident image to be a full copy of the capture gateware, and it was rejected: it would have meant a Duplicator quietly capturing with whatever the capture logic looked like on the day the board was built. A unit that says clearly that it cannot capture is a far better outcome than a unit that captures subtly wrongly.

### Reinstalling it

1. Open **Tools → Firmware → Update firmware…**.
2. Press **Choose update file…** and pick a `.dddfw` update file.
3. Press **Reinstall gateware**.

The same file and the same window as an ordinary update. It takes **a few minutes** — the gateware is written to the FPGA's own memory a byte at a time over an indirect route, and there is no faster way to reach it — and the progress bar pauses for about a second at intervals while a block of that memory is erased. Both are normal, and the window says so while it happens.

As always: **leave the device plugged in and powered.** If it is interrupted, you end up exactly where you started — back in recovery gateware, ready to try again. There is no state between the two.

When it finishes, the Duplicator restarts its FPGA by itself and the Firmware window shows the new gateware version.

### If the button is not offered

Two cases, and the window says which:

**"This device's gateware predates the update mechanism."** The Duplicator was set up before the two-image arrangement existed, so there is no resident image behind the capture gateware and no way for it to replace itself. It needs its gateware written once over the DE0-Nano's own USB connector, using the [hardware programming](../development/hardware-programming/index.md) pages. After that it updates from this window like any other.

**"This device's FPGA is not answering."** Reconnect the Duplicator. If it still does not answer, the same hardware programming pages are the next place to look.

## A brand-new Duplicator

**Use [Bringing up a new or legacy board](bringing-up-a-board.md) instead of this page.** A board you have just built needs its FPGA programmed as well as its FX3, and that wizard does both in one flow, ending with a unit that captures.

The steps above would do only half of it. A newly built FX3 does arrive in recovery mode and this window will happily program it — but the FPGA has never been written, so the result is a board that enumerates and cannot capture, and you would then need the bring-up wizard anyway. Going straight there is one pass instead of two.

What is on this page is the repair for an **interrupted update** on a board that was already working, which is a different thing that happens to look the same on the bus.

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

A Duplicator in recovery mode is running the USB chip's built-in loader, which understands one command: *put these bytes at this address*. The application hands it the firmware out of the update file you chose, tells it to run that, and waits for the Duplicator to reappear — now running its proper firmware, out of memory rather than out of its own storage. From that point it is an ordinary update: the firmware writes the update file into the Duplicator's storage, reads all of it back, checks it against the file, and only then restarts. Nothing about programming a new board takes a shortcut around the checks a routine update makes.

The gateware works the same way and for the same reason. The FPGA's memory is not wired to the USB chip at all, so the firmware reaches it *through the FPGA itself* — which is why the resident image has to be able to talk, and why the whole thing takes minutes rather than seconds. The new gateware is written, read back and checked; only then does the firmware write the twenty-four bytes that tell the FPGA where to find it. Those bytes are the last thing written, so until they are there the FPGA keeps starting the resident image — which is exactly the state an interrupted update leaves, and exactly the state the button above finishes.

The full description of both is on the [Device update mechanism](../development/device-update-mechanism.md) page.
