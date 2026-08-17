# If a capture fails

When a capture stops for a reason, the application names the failure, says what to do about
it, and says where the partial file is. Each of those failures has its own remedy — a full
disk, a bad cable, the kernel's usbfs limit and a machine that cannot keep up are four
different answers, not one.

This page is the same information, gathered, plus the cases where nothing failed but nothing
works either.

!!! success "The file is not lost"

    However a capture stopped, the FLAC stream is closed properly on the way out. What had
    already been written is readable and reports its own length, and the message tells you
    where it is.

## The device is not found

Nothing has failed — there is nothing to fail yet. Work through these in order.

**Is it on a USB 3 port?** A device that enumerated below SuperSpeed appears in the list
marked *connected at insufficient speed*, and is refused rather than opened and left to fail
later. Move it to a USB 3 port, directly on the computer rather than through a hub.

**Linux: are the udev rules installed?** Without them the device node belongs to root, so
enumeration finds the device and opening it fails. This is the single most common cause. See
[Linux device access](../development/hardware-programming/linux-device-access.md), and the
[Flatpak page](install-flatpak.md) for the commands — a Flatpak cannot install host udev
rules for you.

**Windows: is WinUSB bound?** Windows will not let the application talk to the device until
WinUSB is bound to it, once per machine, with Zadig. See the [MSI page](install-msi.md).

**Does it say *recovery mode, no firmware installed*?** Then it was found, and it needs
programming rather than debugging. Go to [If an update fails](if-an-update-fails.md), which
covers a board that has never been programmed as well as one whose update was interrupted.

**Is something else using it?** Only one application can hold the device open. Close the
other one — including another copy of this one.

## This machine cannot keep up

Three different messages, one family of causes.

> This machine could not write the data as fast as the device produced it, and samples were
> lost.

> The device's sequence numbering broke, which means samples were lost. This capture is not
> bit-perfect.

> This machine did not keep a read request outstanding, so the device had nowhere to put its
> data and samples were lost.

The device produces 80 MB/s continuously and cannot be asked to slow down, so anything that
stalls the host for longer than the buffer queue holds costs samples.

**Find out which end is short of capacity** before changing anything. The
[Statistics](statistics.md) panel says:

| Buffer queue | Encoder backlog | The bottleneck | The remedy |
| --- | --- | --- | --- |
| Climbing | Climbing | The FLAC encoder | Lower the **compression** level in the Capture panel |
| Climbing | Near zero | The disk | Capture to a faster drive |
| Both near zero, but a stall still happened | | Something outside the application | Raise the **buffer queue** in Settings, and close whatever else is competing for the machine |

For the third message specifically — *did not keep a read request outstanding* — the setting
to change is **USB transfers** in [Settings](settings.md#usb-transfers), and the thing to
look at is whatever else is competing for the CPU.

The device reports this family of problems on its own lights as well, as an alternating
pattern across the row — see [what the lights on the device say](#what-the-lights-on-the-device-say).

## The kernel's usbfs limit

> The kernel's usbfs memory limit is lower than the buffer queue this capture asked for.

Linux only. Raise the limit, which needs administrator rights:

```bash
sudo sh -c 'echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb'
```

That lasts until the machine is restarted. The project's udev rules set it permanently —
another reason to install them.

Reducing the **buffer queue** size in [Settings](settings.md#buffer-queue) works without
administrator rights, and is the right answer on a machine you do not administer.

## The file could not be written

> The capture file could not be created.

Check that the destination folder exists, that it is writable, and that the volume is not
full. Choosing a different **Folder** in **File ▸ Settings…** is the quickest way to test
this.

On Linux with the Flatpak, a folder outside your home directory and the usual mount points
needs an explicit grant — see the [Flatpak page](install-flatpak.md).

> Writing to the capture file failed.

Check the free space on the destination volume, and that the drive has not been
disconnected. If the volume is full, free some space or capture to a different drive.

## The link or the device

> The connection to the device failed.

Check that the device is plugged in and that no other application is using it. Unplugging it
and plugging it back in resets the device's own state.

> A USB transfer failed.

Try a different cable, and a port connected directly to the computer rather than through a
hub. A capture needs a USB 3 port to carry 80 MB/s at all.

> The device stopped sending data.

Check the cable and the device's power, then unplug it and plug it back in.

## The test pattern did not arrive intact

> The device's test pattern did not arrive intact, so something in the capture path is
> corrupting data.

This is a fault in the hardware or the cabling rather than in the recording — the ramp is
generated in the gateware and is known exactly, so a break in it is data that was corrupted
in transit. Check the cable and the port, then repeat the test capture. See
[Test mode](test-mode.md).

## A fault in the application

> The capture failed because of a fault in this application.

Or any failure the application cannot name. Please report it, with the contents of the Log
panel — start with [`--debug`](command-line.md) to record the full diagnostics, reproduce
the fault, then copy the panel's contents into the report along with the version string from
**Help ▸ About**.

[Submitting a bug report](../support/submitting-a-bug-report.md) says what else is useful.

## What the lights on the device say

The FPGA board has a row of eight small lights, and the Duplicator uses them to report what it
is doing. They are useful here because they come from the device rather than from this
application, so they still answer when the two are not talking to each other at all.

| What you see | What it means |
| --- | --- |
| The two at the **ends** of the row | Connected and idle. This is the normal resting state |
| **All eight** lit | A capture is running |
| **Alternating**, four lit | The device's own buffer is overflowing — see below |
| The **middle two** | A firmware or gateware update is in progress. Leave the device alone |
| **Nothing** lit | The FPGA has no gateware in it. See [Updating your Duplicator](updating-your-domesday-duplicator.md) |
| **One** light, steady, at one end | The FPGA is running but the two boards on the device are not talking to each other |
| **One** light, blinking every few seconds | The FPGA is failing to start and retrying. The device needs its gateware reinstalled |

The alternating pattern is the one to know. It means the device filled its internal buffer
because this machine was not taking data quickly enough — the same cause as [this machine
cannot keep up](#this-machine-cannot-keep-up) above, and the same remedies apply. Seeing it
while a capture runs tells you the bottleneck is between the device and this machine, and not
something in the device itself.

**The lights show trouble that is happening, not trouble that happened.** A brief overflow can
come and go faster than the device updates the row, so a capture can lose samples without the
pattern ever appearing. Do not read a clean row as proof that a capture was clean — the
capture's own result and the [Statistics](statistics.md) panel are the record, and the lights
are the thing you can see from across the room while it is still running.

## Warnings that are not failures

**Running out of space.** The destination volume has less capture time left than the
threshold in the Capture panel. Raised once per capture rather than repeatedly, and it does
not stop anything — the estimate is an estimate, and stopping a capture on a prediction
would sometimes be wrong in the direction that costs a session.

**Firmware version.** The device is running a build that differs from this application's.
Worth mentioning and not known to be broken, so it does not block a capture. If you want the
two to match, see [Updating your Duplicator](updating-your-domesday-duplicator.md).

**Throughput noticeably above 40.00 Msps.** Not an error message, but worth knowing: the
device is clocked by a 40 MHz converter and physically cannot exceed it. A higher figure
means the samples are not coming from the ADC — an unprogrammed FPGA, or gateware that is
not the sampler.
