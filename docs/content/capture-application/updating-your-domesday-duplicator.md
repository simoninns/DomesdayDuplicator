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
2. Open **Help → Firmware…** and choose the **Update** tab.
3. Press **Choose update file…** and pick the `.dddfw` file you downloaded.

The window now tells you what it found:

- the update's version, and what is in it;
- a one-line note from whoever made the release;
- **✓ This update is verified as intact and correctly signed** — the file has been checked against the project's signing key and every part of it has been checked against the digest the release recorded. If that line is missing, something is wrong with the file and the **Update** button will not be available;
- how long it will take, and the one instruction that matters:

> **Leave the device plugged in and powered.**

4. Press **Update**.

## What you will see

The window works through several stages, each with its own progress. They take very different lengths of time, which is why they are shown separately rather than as one bar that would appear to stop.

| Stage | What is happening | Roughly |
| --- | --- | --- |
| **Sending the update to the device** | The file goes over the USB cable, and the Duplicator writes each piece into its own memory as it arrives | Tens of seconds |
| **The device is checking what it wrote** | The Duplicator reads all of it back and checks it against the update file | Tens of seconds |
| **Restarting the device** | It disconnects and reconnects by itself. **This is normal** | Ten seconds or so |
| **Confirming the new version** | The application asks the device what it is now running | Immediately |

The first stage is the slow one, and it is slow because it is doing two things at once: the
Duplicator writes the update as it receives it rather than collecting it first. There is no
separate "writing" step to wait for.

At the end you will see something like:

> **Update complete**
> Your device now reports firmware `89abcdef` and gateware `89abcdef` — update complete.

That last line is read back from the device itself, not assumed from the file. The update is not called finished until the Duplicator has said what it is running and it matches.

## While it is running

Two things, and both matter more than they sound:

- **Leave the device plugged in and powered.** Pulling the cable part way through is the one thing that makes this take longer than it needs to. It cannot break the unit — see below — but you will have to repair it before you can use it.
- **Do not close the window.** If you try, it will explain why not and tell you when it will be safe. If you do want to stop, use the **Stop** button: that ends the update at a point where nothing has been committed, and your device carries on with the software it already had.

Captures cannot run while an update is in progress, and an update cannot start while a capture is running. The two are kept apart by the device itself, not by the window asking you nicely.

## If something goes wrong

**Your device is not damaged.** That is worth stating first because it is true of every way this can fail.

The new software is not made to count until the very last step, after the device has read back everything it wrote and checked it. So an update that stops part way — a pulled cable, a power cut, a closed laptop — leaves the Duplicator either exactly as it was, or in a **recovery mode** that this window recognises and can repair. It never leaves it half-working.

Whatever happens, the window will tell you:

- what happened, in words rather than an error number;
- that the device is safe;
- the one thing to do next, which is usually a single button — **Try again**.

If the message asks you to unplug the device and plug it back in, do that, then reopen **Help → Firmware…** and look at what it reports. If the update did not take, the versions will be the ones you started with and you can simply try again.

If the device comes back saying **recovery mode**, that is the state described above and the repair is a single button. [If an update fails](if-an-update-fails.md) is the page for it — and the same page covers bringing a Duplicator you have just built to life for the first time, which is the same procedure.

## For the curious

- Every update file carries a signature and a checksum for each part of it, and those are checked in the application before anything is sent, checked again by the device as the bytes arrive, and checked a third time by the device reading back what it wrote. The full chain is on the [Device update mechanism](../development/device-update-mechanism.md) page.
- An update signed with the project's *development* key — one you built yourself, or somebody else did — is bannered as such in the window every time. That signature proves the file is well formed and proves nothing whatever about where it came from.
- There is also `ddd-update`, a command-line tool that installs the same file through exactly the same code. It is meant for bench work and scripts; see the [Developer update loop](../development/developer-update-loop.md).
- Going back to an older release is allowed on purpose. The releases page is the archive, and installing an older update file is how you roll back.
