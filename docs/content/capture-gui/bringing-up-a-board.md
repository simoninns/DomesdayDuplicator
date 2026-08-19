# Bringing up a new or legacy board

**Tools ▸ Firmware ▸ Bring up a new or legacy board…**

Programming a Domesday Duplicator from nothing to fully up to date: the FX3's firmware, and both images in the FPGA's configuration flash. When it finishes there is nothing else to do — the board captures.

**It does not matter what the board is running now.** A newly built unit whose FX3 has never been programmed and whose DE0-Nano still holds whatever Terasic shipped on it; a unit running the original Duplicator firmware from before this application existed; a unit that already works; a unit left half-programmed by a run of this that was stopped. This asks for the same things and does the same work in every case, and running it twice is harmless.

The reason is one physical fact: **fitting jumper J4 puts the FX3 into its boot ROM whatever it was doing**, and a JTAG configuration replaces whatever the FPGA is running whatever its flash holds. So nothing here has to diagnose your board, and nothing branches on what it finds. The one exception is a courtesy: a board already sitting in its boot ROM skips the two jumper pages.

If your board already answers this application and you only want the current release on it, **Tools ▸ Firmware ▸ Update firmware…** does that with no cables moved and no case opened. That is [updating](updating-your-domesday-duplicator.md), and it is what you want nearly every time.

## Before you start

**Take the unit out of its enclosure.** The FX3's jumper can be reached with the case on; the DE0-Nano's mini-USB connector cannot, and part of this happens through it.

**Stop any capture first.** The menu entry is greyed out while one is running, because everything behind it resets the device, rewrites its flash or reconfigures its FPGA, and a capture is a bulk transfer from that same device. Monitoring needs nothing from you — opening the wizard puts it down, and closing the wizard picks it up again.

You will need:

| | |
| --- | --- |
| The kit's **USB 3.0 cable** | The one you normally capture through |
| The DE0-Nano's **mini-USB cable** | A cable that carries **data**. A charge-only cable is the commonest thing that goes wrong here, and the board lights up either way |
| A **jumper** (shunt) | Only if the board is running firmware already. A newly built kit is already waiting where the wizard needs it |
| An **update file** | Usually nothing to do: an installed copy of this application already carries one. See *The update file* below |

Both cables stay connected for the whole procedure. Allow about five minutes, most of it watching the three images being written and checked.

## The one thing that catches people

**A power cycle means unplugging *both* cables.**

The assembled unit is powered through the FX3 kit's USB 3.0 connector *and* through the DE0-Nano's mini-USB. Either one alone keeps the whole assembly alive. So pulling just the USB 3.0 cable leaves the unit powered, the FX3 never re-reads where it boots from, the FPGA never reloads from flash — and the board stays lit and looks completely normal while nothing at all has happened.

Every page that asks for a power cycle says *both*, in those words. If a page waits and waits for the board to come back, that is the first thing to check.

## The update file

**The same file the update window installs.** One signed `.dddfw` per release, and it carries four payloads:

| Payload | What it is |
| --- | --- |
| `firmware.img` | The FX3's firmware |
| `gateware-app.rpd` | The application gateware — what the board captures with |
| `gateware-provisioning.svf` | The gateware as **JTAG vectors** |
| `gateware-factory.rpd` | The **factory image**, as raw flash bytes |

An ordinary update writes the first two and ignores the rest. A bring-up needs all four, and the reason is the ordering problem this whole flow exists to solve: an ordinary gateware update goes *through* the gateware's own flash bridge, which a board being brought up does not yet have. So the vectors put a gateware into the FPGA over a second cable first, and everything else is then written through the bridge that gateware provides.

**An installed copy of this application already carries a file**, and the wizard chooses it for you. That is deliberate and it is the point: a board being brought up cannot be updated over USB, so the computer beside it is quite likely one that has just been built and has nothing downloaded yet. Nothing about this procedure needs a network.

Two reasons to choose a file instead, and the page has a button for it:

- **your copy carries none.** A build from source does, unless it was told otherwise. The page says so and names the file to fetch;
- **you have a newer release** than the one your application shipped with.

Either way it is verified identically: the signature first, then every payload's digest, before anything is programmed. Arriving with the application is not a reason to trust a file, and a bundled one that does not verify is refused exactly as a downloaded one would be. A development-signed file says so on the page, every time, because that signature proves the file is well formed and nothing whatever about where it came from.

A release bundle from before the bring-up payloads existed is refused here, and the page names exactly which of the four it is missing. Such a file updates a working device perfectly well; it just cannot bring a board up.

## How each page tells you where it has got to

Every page that has anything to wait for ends in one line, and it is one of two:

| Line | Means |
| --- | --- |
| **• Waiting for** *…something…* | Nothing is happening yet, and this is what it is waiting for — you to press a button, or a board to come back |
| **✓ All done.** *…what happened…* **Click “Next ›” to continue.** | That step is finished |

A step that is finished also takes its own button away: **Load the gateware** becomes a greyed-out **Gateware loaded**, and **Program the board** becomes **Board programmed**. So there is never a question of whether pressing it again is needed, or whether it worked the first time.

## What the wizard does, page by page

### 1 · What this does, and what it will ask of you

A checklist rather than an explanation: what to have ready before starting, and what you will be asked to do. Everything physical is listed here so that the case comes off once, both cables go on once, and no physical step is ever asked for twice.

It also states where you end up: **a working Duplicator running the release you chose, ready to capture.** Nothing follows it.

### 2 · Both boards, connected

Two live status rows, one per board. Each device is **opened**, not merely noticed, so that a permissions problem turns up here — while nothing is half-programmed — rather than in the middle of writing a flash.

These rows inform; they decide nothing. Every state below is one the wizard can bring up.

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
| *Running this application's own firmware (commit)* | A board that already works. **You probably want [Update firmware](updating-your-domesday-duplicator.md) instead.** Carrying on is safe and reprograms everything | Amber |
| *Running the **original** Duplicator firmware … 1d50:603b* | The firmware from before this application existed | Amber |
| *The kit's debug serial port is answering* | The board has power and its USB 3.0 link is not answering. Check that cable and that it is in a USB 3.0 socket | Red |
| *Nothing found* | No cable, no power, or no permissions | Red |

And the FPGA row:

| Row says | Meaning | Mark |
| --- | --- | --- |
| *Found and opened* | The USB-Blaster is reachable | Green |
| *Nothing found* | Nearly always a charge-only cable. On Linux, check `70-altera-usb-blaster.rules` is installed — see [Linux device access](../development/hardware-programming/linux-device-access.md) | Red |
| *attached but could not be opened* | Something else has it. Quartus's own `jtagd` holds the cable open whenever it is running; on Windows it is the driver binding | Red |

**Bring-up needs both device rules files installed on Linux** — the Duplicator's and the USB-Blaster's.

### 3 · The update file

The file your copy of the application carries, already chosen, with its version and what it carries written out. Signature and digests are checked here, on that one exactly as on any file you choose instead.

If your copy carries none, this is the one page that needs something from elsewhere: the file to download and where from.

### 4 · Fit jumper J4

Skipped for a board already in its boot ROM.

Three numbered instructions: fit the jumper across the FX3 board's two-pin `PMODE` header, unplug both cables, plug both back in. A photograph shows exactly which header. The jumper only takes effect on a boot, and the unit does not boot while either cable still feeds it — which is why all three steps are needed and why doing only the first looks exactly like success.

The page waits for the board to come back in its boot ROM, and says so until it does.

This is the page that makes your board's previous state irrelevant, and it is the only page that cares what that state was.

### 5 · Load the gateware into the FPGA

Press **Load the gateware**. It plays the gateware into the FPGA through the DE0-Nano's own USB-Blaster — the only route to a board whose flash holds nothing this application can talk to. About three seconds.

**Nothing is written to the board by this step.** A JTAG configuration lives in the FPGA's own memory and would be lost the moment the power went off. What it buys is the next page: a Duplicator that can reach its own flash, so that everything after this is written by the device itself over the USB 3.0 cable.

The FX3 is sitting in its boot ROM while this happens, with every shared pin idle. That is why this comes before the firmware rather than after it — see *Why this order* below.

### 6 · Program the board

Press **Program the board**. Everything permanent happens in this one step: the FX3's boot ROM is handed the firmware and runs it from memory, and that firmware then writes three things — the same protocol, the same digests and the same readback an ordinary update uses:

1. its own **EEPROM**, so the board has firmware of its own;
2. the FPGA's **factory image**, which is what the board falls back to and what makes future updates possible;
3. the FPGA's **application image**, which is what it captures with.

**That order is deliberate.** Stopped or interrupted at any point, the board comes back in a state this application can reach and repair: after the first write it boots the new firmware, and after the second it always has a valid image to fall back to before the region it falls back *from* is touched. Written the other way round, an interrupted run could leave a bare board with an application image and nothing to load it with.

Nothing is restarted at the end. The jumper is still fitted, and a restart would land the FX3 back in its boot ROM — so all three become the running images together, at the power cycle two pages on.

The flash writes pause every few seconds while a block is erased; that is the flash doing its job, not something stuck. **Stop** is safe at any point.

### 7 · Remove jumper J4

Same board, same photograph, header bare. **Do not unplug anything yet.**

Nothing here can detect a jumper, and the page says so rather than pretending to wait for something: click **Next ›** once it is off.

### 8 · Power cycle

Three numbered instructions again: unplug both, wait a couple of seconds, plug both back in. That is what makes the three images you have just written the running ones — the FX3 re-reads where it boots from, and the FPGA loads the factory image, which hands over to the application image beside it.

**This page cannot go by whether a Duplicator is attached**, and that is worth knowing because one is. The previous step handed the firmware to the FX3's boot ROM and ran it out of memory, so the board is enumerating as a working Duplicator before you touch anything. What the page waits for instead is two things it can observe:

- the device **going away**, which is what proves a cable came out;
- it coming back **on the application image**, which is what proves the board lost power. The gateware loaded over JTAG in step 5 is the *factory* image, and only a power cycle makes the FPGA reload from flash.

That second check catches the failure this page has always warned about. Pulling the USB 3.0 cable alone makes the device vanish and return while the mini-USB keeps the board alive — the firmware in memory survives, the gateware in the FPGA survives, and every outward sign is of a power cycle that worked. The page says so:

| What the page says | What happened |
| --- | --- |
| *Waiting for both cables to come out* | Nothing yet — the board is still on the bus |
| *Waiting for the Duplicator to come back* | The cables are out |
| **The board did not lose power** | Only one cable came out. Unplug **both** |
| **The board has come back in its boot ROM** | Jumper J4 is still fitted. Go back a page and take it off |
| **✓ All done** | It restarted and is running from its own flash |

### 9 · What the device is running now

Four things, read off the device rather than assumed:

- the Duplicator's own firmware is running;
- it speaks a protocol this build knows;
- its firmware is the build the update file carries;
- its FPGA is answering, **on the application gateware**.

That last one is what separates a finished board from one that is most of the way there. A board that comes back on its factory image works and is not damaged, but it cannot capture — and the page says so rather than reporting success.

Then: **put the case back on and click Close.** From now on this board updates itself.

## Why this order

The FX3 and the FPGA share one interconnect, and one line changed direction between the original design and the current one. Under the original firmware `CTL_07` is driven by the FX3; under the current gateware the same wire is driven by the FPGA. Those two must never run together.

What keeps a board out of that pairing is not care — it is where the FX3 is while the FPGA changes. In its boot ROM every shared pin is idle, so the gateware can change underneath it with nothing to contend with, and by the time any firmware runs again it is the firmware out of the file you chose. That is why the jumper page comes before the gateware is loaded, and why the wizard refuses to write anything until it has been.

## If something goes wrong

**Nothing here can be broken by stopping part way**, and that is worth stating plainly:

- the FX3 cannot be bricked. Fitting jumper J4 always reaches its boot ROM, whatever is or is not in its EEPROM;
- the DE0-Nano cannot be bricked. JTAG always reaches the configuration flash, whatever is in it;
- every step can simply be run again, from the beginning, as many times as you like. A stopped run leaves a board this same wizard accepts on its next run, because that run assumes nothing.

The commonest failures, in order:

| What you see | What it usually is |
| --- | --- |
| A page waiting for a board that never comes back | Only one cable came out. Unplug **both**, count to three, reconnect |
| *The board did not lose power* on page 8 | The same thing, caught rather than guessed at — the USB 3.0 cable alone came out while the mini-USB kept the board alive |
| *Nothing found* on the FPGA row | A charge-only mini-USB cable |
| The cable is attached and will not open | Quartus's `jtagd` is running, or the udev rules are not installed |
| A page waiting for the boot ROM after fitting the jumper | The jumper is on the wrong header, or the power cycle did not happen |
| The file is refused on page 3 | It is a release from before the bring-up payloads existed. The page names what is missing |

## What this replaces

Before this existed, both halves were programmed with vendor tools: `fx3-programmer` for the firmware and Quartus for the FPGA — several gigabytes of unfree, x86_64-Linux-only software whose entire role was to write a file this project already publishes. Those procedures still work and are still documented under [hardware programming](../development/hardware-programming/index.md); the wizard is the same operations without the toolchain.
