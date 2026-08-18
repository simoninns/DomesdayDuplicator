# Bringing up a new or legacy board

**Tools ▸ Firmware ▸ Legacy ▸ Bring up a new or legacy board…**

Programming both halves of a Domesday Duplicator from nothing: the FX3's firmware and the FPGA's configuration flash. Two boards need this and no others —

- a **newly built** unit, whose FX3 has never been programmed and whose DE0-Nano still holds whatever Terasic shipped on it;
- a unit running the **original Duplicator firmware**, from before this application existed. That firmware has no way to receive an update, so the ordinary [update](updating-your-domesday-duplicator.md) path cannot reach it.

Everything else — a device that already answers this application — is updated from **Tools ▸ Firmware ▸ Update firmware…** and needs nothing on this page.

## Before you start

**Take the unit out of its enclosure.** The FX3's jumper can be reached with the case on; the DE0-Nano's mini-USB connector cannot, and half of this happens through it.

You will need:

| | |
| --- | --- |
| The kit's **USB 3.0 cable** | The one you normally capture through |
| The DE0-Nano's **mini-USB cable** | A cable that carries **data**. A charge-only cable is the commonest thing that goes wrong here, and the board lights up either way |
| A **jumper** (shunt) | Only if the board is running firmware already. A newly built kit is already waiting where the wizard needs it |
| A **provisioning set** | Usually nothing to do: an installed copy of this application already carries one. See *The provisioning set* below |

Both cables stay connected for the whole procedure. Allow about five minutes, most of it watching the FX3's firmware being written and checked.

## The one thing that catches people

**A power cycle means unplugging *both* cables.**

The assembled unit is powered through the FX3 kit's USB 3.0 connector *and* through the DE0-Nano's mini-USB. Either one alone keeps the whole assembly alive. So pulling just the USB 3.0 cable leaves the unit powered, the FX3 never re-reads where it boots from, the FPGA never reloads from flash — and the board stays lit and looks completely normal while nothing at all has happened.

Every page that asks for a power cycle says *both*, in those words. If a page waits and waits for the board to come back, that is the first thing to check.

## The provisioning set

A provisioning set is an ordinary signed update file with a different payload in it: the FX3 firmware, the FPGA's **factory image**, and the **JTAG vectors that load it**. Two gateware payloads rather than one, and the pair is the point: the ordinary gateware update goes *through* the gateware's own flash bridge, which a board being brought up does not yet have — so the vectors put a gateware into the FPGA first, and the image is then written through the bridge that gateware provides.

**An installed copy of this application already carries one**, and the wizard chooses it for you. That is deliberate and it is the point: a board being brought up cannot be updated over USB, so the computer beside it is quite likely one that has just been built and has nothing downloaded yet. Nothing about this procedure needs a network.

Two reasons to choose a file instead, and the page has a button for it:

- **your copy carries none.** A build from source does, unless it was told otherwise. The page says so and names the file to fetch;
- **you have a newer set** than the one your application shipped with — bring-up works from any set, and the one that came with your copy is from the release it was packaged alongside.

Either way it is verified identically: the signature first, then every payload's digest, before anything is programmed. Arriving with the application is not a reason to trust a file, and a bundled set that does not verify is refused exactly as a downloaded one would be. A development-signed set says so on the page, every time, because that signature proves the file is well formed and nothing whatever about where it came from.

An ordinary update file chosen here is refused with a sentence saying so — it carries no vectors, so it cannot bring up an FPGA.

## What the wizard does, page by page

### 1 · What this does, and what it will ask of you

Everything physical, listed before anything starts. That is deliberate: the case comes off once, both cables go on once, and no physical step is ever asked for twice.

It also states where you end up, which is worth reading: **your Duplicator will be running current firmware with the recovery gateware.** That is a working, enumerating device that cannot capture yet. One ordinary firmware update finishes it, and that update needs no cables moved and no case opened.

### 2 · Both boards, connected

Two live status rows, one per board. Each device is **opened**, not merely noticed, so that a permissions problem turns up here — while nothing is half-programmed — rather than in the middle of writing a flash.

Each row carries a mark, and the middle one is the one people misread:

| Mark | Means |
| --- | --- |
| Green tick | That board is ready as it is |
| **Amber dot** | The wizard will ask you to do something to that board later on — fit a jumper, or pull both cables. **Not** that anything is wrong with it |
| Red cross | Something to put right before going on |

What the FX3 row can say:

| Row says | Meaning | Mark |
| --- | --- | --- |
| *Waiting in its boot ROM* | A newly built kit. No jumper needed; the wizard skips those pages | Green |
| *Running this application's own firmware (commit)* | A board that already works. **You probably want [Update firmware](updating-your-domesday-duplicator.md) instead** — bring-up reinstalls both halves and will send you to the jumper. Carrying on is safe | Amber |
| *Running Duplicator firmware that predates this application's update agent* | The current identifiers, firmware too old to update itself. One of the two boards this wizard is for | Amber |
| *Running the **original** Duplicator firmware … 1d50:603b* | The firmware from before this application. The other board this wizard is for | Amber |
| *The kit's debug serial port is answering* | The board has power and its USB 3.0 link is not answering. Check that cable and that it is in a USB 3.0 socket | Red |
| *Nothing found* | No cable, no power, or no permissions | Red |

And the FPGA row:

| Row says | Meaning | Mark |
| --- | --- | --- |
| *Found and opened* | The USB-Blaster is reachable | Green |
| *Nothing found* | Nearly always a charge-only cable. On Linux, check `70-altera-usb-blaster.rules` is installed — see [Linux device access](../development/hardware-programming/linux-device-access.md) | Red |
| *attached but could not be opened* | Something else has it. Quartus's own `jtagd` holds the cable open whenever it is running; on Windows it is the driver binding | Red |

**Bring-up needs both device rules files installed on Linux** — the Duplicator's and the USB-Blaster's.

### 3 · The provisioning set

The set your copy of the application carries, already chosen, with what it is and what it carries written out. Signature and digests are checked here, on that one exactly as on any file you choose instead.

If your copy carries none, this is the one page that needs something from elsewhere: the file to download and where from.

### 4 · Fit jumper J4

Skipped for a board already in its boot ROM.

Fit the jumper across the FX3 board's two-pin `PMODE` header, then **unplug both cables and reconnect them**. The jumper only takes effect on a boot, and the unit does not boot while either cable still feeds it. A photograph shows exactly which header.

### 5 · Program the FX3

The boot ROM is handed the firmware, runs it from memory, and that firmware then writes and checks its own EEPROM — the same protocol, the same digests and the same readback an ordinary update uses.

The device is deliberately **not restarted** at the end of this step: the jumper is still fitted, so a restart would land it back in its boot ROM. The restart comes later.

### 6 · Remove jumper J4

Same board, same photograph, header bare. Do not unplug anything yet.

### 7 · Program the FPGA

Two things behind one page, and the first makes the second possible.

The **factory image** is loaded into the FPGA through the DE0-Nano's own USB-Blaster — the only route to a board with no working gateware. That takes about three seconds and **writes nothing**: a JTAG configuration lives in the FPGA's own memory and lasts until the power goes off.

The Duplicator can now reach the FPGA's flash for itself, so it writes that same image into it, exactly as it writes an ordinary gateware update. The factory image is the one the board falls back to, and the one that makes those updates possible from then on.

Expect under a minute for both. The flash write pauses every few seconds while a block is erased — that is the flash doing its job, not something stuck. **Stop** is safe at any point: a partly written flash is not a broken one, because nothing boots from it until the power cycle, and the whole step can simply be run again.

### 8 · Power cycle

One cycle, discharging two obligations: the FX3 has to re-read where it boots from, and the FPGA has to drop the configuration JTAG gave it and load the one that was just written to flash.

**Both cables.** See above.

### 9 · What the device is running now

Four things, read off the device rather than assumed:

- the Duplicator's own firmware is running;
- it speaks a protocol this build knows;
- its firmware is the build the provisioning set carries;
- its FPGA is answering, on the recovery gateware.

Then the last step: **Tools ▸ Firmware ▸ Update firmware…** with the current release bundle, which installs the capture gateware. There is a button on the page that opens it. Nothing after this point is physical, so the case can go back on first.

## If something goes wrong

**Nothing here can be broken by stopping part way**, and that is worth stating plainly:

- the FX3 cannot be bricked. Fitting jumper J4 always reaches its boot ROM, whatever is or is not in its EEPROM;
- the DE0-Nano cannot be bricked. JTAG always reaches the configuration flash, whatever is in it;
- every step can simply be run again, from the beginning, as many times as you like.

The commonest failures, in order:

| What you see | What it usually is |
| --- | --- |
| A page waiting for a board that never comes back | Only one cable came out. Unplug **both**, count to three, reconnect |
| *Nothing found* on the FPGA row | A charge-only mini-USB cable |
| The cable is attached and will not open | Quartus's `jtagd` is running, or the udev rules are not installed |
| A page waiting for the boot ROM after fitting the jumper | The jumper is on the wrong header, or the power cycle did not happen |

## What this replaces

Before this existed, both halves were programmed with vendor tools: `fx3-programmer` for the firmware and Quartus for the FPGA — several gigabytes of unfree, x86_64-Linux-only software whose entire role was to write a file this project already publishes. Those procedures still work and are still documented under [hardware programming](../development/hardware-programming/index.md); the wizard is the same operations without the toolchain.

## The reverse

[Rolling back to legacy firmware](rolling-back-to-legacy-firmware.md) undoes all of this, and takes about two minutes over one cable with the case on. The two flows are a closed loop: a unit can be rolled back and brought up again as often as anybody likes.
