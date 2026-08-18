# Rolling back to legacy firmware

**Tools ▸ Firmware ▸ Legacy ▸ Roll back to legacy firmware…**

Putting the **original** Domesday Duplicator firmware and gateware back on a working unit —
the software it shipped with, from before this application existed. Two reasons to want it:

- you need the **original software ecosystem**, and the tools that go with it;
- you are **testing the bring-up flow** against a genuinely legacy board, which is what this
  turns any current unit into.

It is a deliberate act and it is reversible. It is not reversible from here.

## Before you start

Nothing physical. **One cable — the USB 3.0 one you already capture through — and the case
stays on.** That is worth stating plainly because the flow that undoes this does not: it
needs the case off, the DE0-Nano's mini-USB cable and a jumper moved twice.

The reason for the asymmetry is the whole point of the exercise. A unit you can roll back is
a *working* unit: it runs firmware that can write its own EEPROM and gateware that can
rewrite its own flash, so it does the whole job itself over one link. The original firmware
can do neither, which is why bringing it back needs a JTAG cable.

You will need a **legacy rollback file** — `domesday-duplicator-legacy-rollback-*.dddfw`,
published alongside each firmware release. Unlike a provisioning set, it is not bundled with
the application: rolling back is a rare, deliberate act and the file picker serves it.

Allow about two minutes.

## What you lose

| | |
| --- | --- |
| **This application** | A rolled-back unit appears on the bus as a different device. It cannot capture, examine, or be driven from here at all |
| **Updating** | The original firmware predates the update mechanism entirely — there is nothing on the device to receive an update. This is exactly why coming back is harder than going |
| **The register interface** | Test-data mode, the gateware's identity, the flash bridge: all of it |

What you do **not** lose: anything physical, and anything on your discs or captures. The
board is not modified and cannot be damaged by this.

## What the wizard does, page by page

### 1 · What you are about to give up

Everything above, and a field to type **ROLL BACK** into. A typed word rather than a
checkbox, because this is the one thing the application does that the application cannot
undo.

### 2 · The Duplicator, connected

One live status row. The device is **opened**, not merely noticed, so a permissions problem
turns up here rather than in the middle of a flash write.

Four things are refused, each by name:

| Row says | Why |
| --- | --- |
| *Already running the original firmware* | There is nothing to roll back. [Bring up a new or legacy board](bringing-up-a-board.md) is the flow that brings it forward |
| *Not running its own firmware* | The device does its own writing, so it has to be working first |
| *The FPGA is not answering* | The original gateware is written **through** the current gateware's flash bridge. No bridge, no route |
| *This gateware predates the flash bridge* | Same reason, one step further back. There is nothing here to roll back from |

### 3 · The rollback file

Signature first, then every payload's digest — the same checks, in the same order, as any
other update file. An ordinary update file chosen here is refused with a sentence saying so,
and the reverse is true too: a rollback file cannot be installed from **Update firmware…**,
because what it does is not an update.

A development-signed file says so on the page, every time.

### 4 · Write the original gateware

**The FPGA goes first, and the order is not a preference.** The original firmware and the
current gateware both drive one wire between the two boards, so the firmware has to be the
last thing to change. While this runs, the unit is still running the firmware that is doing
the writing.

The original gateware goes to the start of the configuration flash, over the factory image,
through the current gateware's own flash bridge — exactly as an ordinary gateware update is
written. It pauses every few seconds while a block is erased. Nothing changes about what the
unit is *running*.

### 5 · Write the original firmware

An ordinary update transfer: no jumper, no boot ROM, nothing to move. The firmware running
now is its own flasher, and this is the last thing it does.

The device is deliberately **not** restarted at the end. Both halves become the running ones
at the power cycle, or neither does.

### 6 · Power cycle

Unplug the USB 3.0 cable, wait a couple of seconds, plug it back in. This is the moment the
unit becomes a legacy Duplicator.

If the DE0-Nano's mini-USB cable happens to be connected, **that one has to come out too** —
either cable alone keeps the assembly powered, and a unit that never lost power never
reboots. That is the commonest reason this page waits and waits.

### 7 · What the device is running now

The one thing a legacy device can be asked: that it enumerates as one. The page then says
what this application can and cannot do with it from here, and points at the flow that
brings it back.

## Coming back

**Tools ▸ Firmware ▸ Legacy ▸ Bring up a new or legacy board…**, which puts the unit back
exactly as it was. Read [Bringing up a new or legacy board](bringing-up-a-board.md) before
you roll back rather than after: it needs the case off and a jumper moved, and it is the
half of this loop that takes fifteen minutes rather than two.

The loop is closed and can be run as often as you like — roll back, bring up, roll back — on
the same unit, forever. That is not a side effect; it is how the bring-up flow gets tested
against a real legacy board without keeping one.

## If something goes wrong

**Nothing here can be broken by stopping part way.** What has been written does not take
effect until the power cycle, so a run stopped half way leaves a unit that still boots and
still works exactly as it did. Every step can simply be run again.

| What you see | What it usually is |
| --- | --- |
| A page waiting for a device that never comes back | The mini-USB cable is still connected, so the unit never lost power |
| *This device's firmware predates the rollback path* | The device is running a firmware release older than the rollback target. Update it first, then roll back |
| The device is refused on page 2 | Read the row — it names which of the four cases it is |

## Where the images come from

The two payloads are **frozen binaries**, generated once from the last commit whose firmware
and gateware present the original USB identity, and committed to the repository with their
digests and their provenance. Nothing rebuilds them and no workflow ever checks out that
tree. The record is in `legacy/README.md`.
