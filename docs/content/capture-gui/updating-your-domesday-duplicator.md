# Updating your Domesday Duplicator

Your Duplicator has software of its own inside it, and from time to time there is a newer version. This page is how you install one.

You need the Duplicator, its USB cable, and a few minutes. No tools, no jumpers, no second cable, and nothing to open.

!!! note "This page describes the file-picker route"

    At the moment you download an update file yourself and choose it in the application. A later release adds a **Check for updates** button that finds and downloads it for you. Everything after choosing the file — the checking, the installing, the confirmation — is already exactly as described here, and does not change when that button arrives.

## What is in an update

Three separate things can be out of date, and the update window shows all three so you can see at a glance which:

| | What it is | How it is updated |
| --- | --- | --- |
| **Application** | The program you are looking at | Through wherever you installed it from — Flathub, or the installer download for Windows and macOS |
| **Firmware** | The program inside the Duplicator's USB chip | Here, in this window |
| **Gateware** | The logic inside the Duplicator's programmable chip | Here, in this window, once your unit supports it |

The application is in the list even though this window cannot install it, because "am I up to date" is a question about all three. If an update file needs a newer application than the one you are running, the window says **update the application first** and will not let you continue — so the order you have to do things in is the only order the window allows.

## Getting an update file

Update files come from the project's releases page and are named like this:

```
domesday-duplicator-update-1.5.0.dddfw
```

Download one and put it somewhere you can find it again.

## Installing it

1. Plug the Duplicator in. If a capture is running, stop it — the device will not update while it is capturing, and it will say so.
2. Open **Tools → Firmware → Update firmware…** and choose the **Update** tab.
3. Press **Choose update file…** and pick the `.dddfw` file you downloaded.

The window now tells you what it found:

- the update's version, and what is in it;
- a one-line note from whoever made the release;
- **✓ This update is verified as intact and correctly signed** — the file has been checked against the project's signing key and every part of it has been checked against the digest the release recorded. If that line is missing, something is wrong with the file and the **Update** button will not be available;
- how long it will take, and the one instruction that matters:

> **Leave the device plugged in and powered.**

Underneath all that, the window lists **every step the update will take**, greyed out, with an empty circle against each. That list is the whole procedure: nothing else will happen, and no step in it will be skipped. Read it before you start — it is what pressing **Update** commits you to.

4. Press **Update**.

## What you will see

The list stops being a plan and starts being a report. One step at a time:

- the step being worked on is **picked out in bold** with a ▶ against it;
- each step that finishes gets a ✓ and stays on screen;
- the steps still to come stay greyed;
- one progress bar underneath fills once, across the whole update, and says which step it is on — *Step 3 of 5*. It never restarts and never goes backwards;
- one line under the bar says what the device is doing this second.

An ordinary update carrying both halves has five steps:

| Step | What is happening | Roughly |
| --- | --- | --- |
| **Check the update file** | The signature and every digest, before anything is sent anywhere | Immediately |
| **Install the firmware** | The file goes over the USB cable, the Duplicator writes each piece into its own memory as it arrives, and then reads all of it back and checks it | Tens of seconds |
| **Install the gateware** | The same again for the programmable chip | A few minutes |
| **Restart the device** | It disconnects and reconnects by itself. **This is normal** | Ten seconds or so |
| **Confirming the new version** | The application asks the device what it is now running | Immediately |

An update that carries only firmware has no gateware step, and a device being brought to life for the first time has an extra one at the start — **Start the device up**. What the list shows is always what this file will do to this device.

**An update carrying gateware takes minutes rather than seconds**, and the reading-back is the
longest part of it — around a minute where the sending took twenty seconds, because the route
to that chip's memory is much faster to write than to read back. The bar is weighted for that,
so the gateware step is most of its width rather than one fifth of it. It will also pause for
a second or so at intervals while a block of that memory is erased, and it holds still while
the device restarts, because nothing there can be measured and the window will not invent
motion it cannot account for. Both are normal. The estimate before you start is deliberately
pessimistic, so finishing early is the usual outcome.

### If you want to see more

**Show details** opens a running log of everything the device reported, each line stamped with how long the update had been going. Nothing in it is needed for an ordinary update — the five steps and the line under the bar are the whole story — but it is the thing to open if something goes wrong, and the thing to copy into a bug report.

At the end you will see something like:

> **Update complete**
> Your device now reports firmware `89abcdef` and gateware `89abcdef` — update complete.

That last line is read back from the device itself, not assumed from the file. The update is not called finished until the Duplicator has said what it is running and it matches.

## While it is running

Two things, and both matter more than they sound:

- **Leave the device plugged in and powered.** Pulling the cable part way through is the one thing that makes this take longer than it needs to. It cannot break the unit — see below — but you will have to repair it before you can use it.
- **Do not close the window.** If you try, it will explain why not and tell you when it will be safe. If you do want to stop, use the **Stop** button: that ends the update at a point where nothing has been committed, and your device carries on with the software it already had.

Captures cannot run while an update is in progress, and an update cannot start while a capture is running. The two are kept apart by the device itself, not by the window asking you nicely.

### The lights on the board

The FPGA board has a row of eight small lights, and the Duplicator uses them to say what it is
doing. During an update **the middle two of the row are lit**, and no other state of the device
looks like that — it is the only pattern lit from the centre, which is what makes it readable
from across a room.

This is worth knowing because it comes from the device rather than from the window. If the
screen has locked, the window is behind something else, or you have simply walked away and come
back unsure whether to touch the cable, those two centre lights are the device's own answer to
"is it still busy". While they are lit, leave it alone.

The one exception is the **Restarting the device** stage, where the lights will change and may
go dark for a few seconds. That is the device reloading itself and it is expected. If they
settle into a pattern lit at *both ends* of the row, the device has come back up and is ready.

## If something goes wrong

**Your device is not damaged.** That is worth stating first because it is true of every way this can fail.

The new software is not made to count until the very last step, after the device has read back everything it wrote and checked it. So an update that stops part way — a pulled cable, a power cut, a closed laptop — leaves the Duplicator either exactly as it was, or in a **recovery mode** that this window recognises and can repair. It never leaves it half-working.

Whatever happens, the window will tell you:

- what happened, in words rather than an error number;
- that the device is safe;
- the one thing to do next, which is usually a single button — **Try again**.

The step list stays on screen with a ✕ against the step it stopped on, and the steps before it keep their ticks. That matters on an update carrying both halves: "the firmware went in and the gateware did not" is a different situation from "nothing happened", and the list is where you can see which of the two you are in. **Show details** has the full account if you want it.

If the message asks you to unplug the device and plug it back in, do that, then reopen **Tools → Firmware → Update firmware…** and look at what it reports. If the update did not take, the versions will be the ones you started with and you can simply try again.

If the device comes back saying **recovery mode**, that is the state described above and the repair is a single button. [If an update fails](if-an-update-fails.md) is the page for it — and the same page covers bringing a Duplicator you have just built to life for the first time, which is the same procedure.

## For the curious

- Every update file carries a signature and a checksum for each part of it, and those are checked in the application before anything is sent, checked again by the device as the bytes arrive, and checked a third time by the device reading back what it wrote. The full chain is on the [Device update mechanism](../development/device-update-mechanism.md) page.
- An update signed with the project's *development* key — one you built yourself, or somebody else did — is bannered as such in the window every time. That signature proves the file is well formed and proves nothing whatever about where it came from.
- There is also `ddd-update`, a command-line tool that installs the same file through exactly the same code. It is meant for bench work and scripts; see the [Developer update loop](../development/developer-update-loop.md).
- Going back to an older release is allowed on purpose. The releases page is the archive, and installing an older update file is how you roll back.
