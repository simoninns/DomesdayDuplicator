# Player control

The application can drive a LaserDisc player over its serial port: read what disc is in it,
work out how long the side is, and then capture that side by itself — spinning the disc up,
watching the address go by, and stopping both the capture and the player at the end.

It is off until you turn it on, and while it is off **no serial port on your machine is
opened, written to, or even listed**.

## Turning it on

**Player → Player control**, or the *Look for a LaserDisc player* box in
**Player → Player settings…**.

The rest of that settings page is optional and every field defaults to "work it out":

| Setting | Default | When to change it |
| --- | --- | --- |
| Player model | Whichever answers | Never, really. The player says which model it is, so this is a check rather than a choice — set it and you will be told if something else answers |
| Serial port | Find it automatically | When you have other equipment on the machine's serial ports. A port chosen here is the only one that will ever be opened |
| Speed | Work it out | When your player's rate is fixed and you would rather not have it searched for |
| Ports never to open | Empty | The other half of the same concern — see below |

Once a player has been found, the port and the speed it answered at are remembered, and the
next launch goes straight there.

## What searching for a player actually does

Worth knowing before you switch it on, because it is the one part of this application that
reaches outside itself.

A search opens each serial port in turn and writes a few bytes to it — the Pioneer model
request — at each speed a player can be set to, and listens for an answer. On a machine
where a serial port is a UPS, a scientific instrument, or something that reprograms itself
when it sees the right bytes, that is not free.

So:

- It only happens when player control is **explicitly enabled**.
- The remembered port is tried first, so a machine that has already found its player
  **scans nothing**.
- A port you name in **Serial port** is the only one ever opened.
- Ports listed in **Ports never to open** are skipped, and that list is remembered.
- A port that is busy is skipped rather than fought over.

If something answers and is not a player, the application says so and names the port, so you
can add it to the exclusion list rather than having it probed every time.

## When the player is not found

The message under the player panel says which of these it is. In rough order of likelihood:

**You are not allowed to open the port.** The commonest first run there is, and it looks
like a fault in the cable if you do not know to expect it.

- *Linux*: serial devices belong to a group — `dialout` on Debian, Ubuntu and Fedora, `uucp`
  on Arch and its derivatives. `sudo usermod -a -G dialout $USER`, then **log out and back
  in**. A group you have just been granted does not apply to the session you are already in,
  which is why "I added myself and it still does not work" is so common. This applies to the
  Flatpak too — the sandbox keeps your groups, it does not route around them.
- *macOS*: not really a permission problem, since `/dev/cu.*` is open to everybody. It is
  the driver. An adapter using the built-in FTDI or CH34x support appears as
  `/dev/cu.usbserial-…` and needs nothing installed; one that needs a vendor driver also
  needs that driver **allowed** in *System Settings → Privacy & Security* after you install
  it, and until it is, its port either does not appear or will not open.
- *Windows*: no group to join. A COM port can only be held by one program at a time, so
  close any terminal or logging tool that might have it, and check *Device Manager → Ports
  (COM & LPT)* for the adapter's driver and its port number.

**Nothing answered anywhere.** The player is off, its serial port is disabled in its own
setup menu, or the cable is wrong: Pioneer industrial players generally take a
straight-through cable to a PC, while several consumer decks want a null-modem. If nothing
answers at any speed on any port, doubt the cable before anything else.

**Something answered and it was not a player.** Named with the port and what it said. Add
the port to the exclusion list.

**An unrecognised player.** It connects anyway, using the standard Pioneer command set, and
the panel shows the model code it reported. Most controls will work and some may not — and
that model code is exactly what is needed to write a definition for it, so it is worth
reporting.

## The remote

**Player → Remote control…** opens a window with the transport controls, the search
controls, the audio and speed selectors, and a field for sending a command by hand.

It is not modal — you can drive the player while watching the spectrum, which is the whole
point of it. A control your model does not have is greyed out and its tooltip names the
models that do have it. Losing the player greys the window out rather than closing it.

The manual command field shows what went out beside what came back, refusals included. It is
the fastest way to find out what a player does with a command this application does not
offer, and its answers are what a new model definition gets written from.

!!! note "The user codes are informational"
    A disc's user codes are read and shown, and nothing is ever derived from them — not the
    length, not the side, not the start or the end. Their field meanings are inferred rather
    than documented, and they are absent on perfectly healthy discs. The one that matters:
    the Pioneer code is not a query. The player searches to the lead-in to answer it, which
    on an LD-V4300D takes about eleven seconds and leaves the disc parked at the start.

## Examining a disc

**Player → Examine disc…**, then **Examine**. It takes about a minute, and it spins the
disc.

What it establishes, and how:

| | Where it comes from |
| --- | --- |
| Whether there is a disc, and whether the tray is open | The player, asked |
| CAV or CLV, and therefore whether the disc is addressed by frame or by time | The disc's own programme status |
| Disc size, side, and whether it has chapters | The same reply |
| NTSC or PAL | The TV system request — the disc's standard, not the one the player is outputting, which are different answers on a converting player |
| The first and last addresses of the programme | **Measured**, by seeking past each end and reading back where the player actually stopped |
| Both user codes | The player, asked |

Every line of the report says where its answer came from, so a measurement and a guess do
not look alike, and anything that could not be established reads *not known* rather than as
a blank or a zero. A player that refuses half of these still produces a usable report with
the other half in it.

The report is copyable — it is meant to be pasted into an issue — and the examination leaves
the disc held still at the start of the side rather than playing.

## Capturing a side by itself

From the examine report, **Set up capture…**. The plan is built from the disc that was just
examined, so the addresses are in the units that disc actually uses and the bounds are the
ones that were just measured.

Three shapes, and they are the three a player can actually be asked for:

**The whole side, spin-up to spin-down.** The capture starts with the disc stopped, so the
spin-up is in the file; and the disc is spun down again before the capture stops, so the
run-out is in it too. Neither of those is an address, and this is the only way either
reaches a file — which is also why this shape stops the player *before* it stops the
capture, where the other two do it the other way round.

**From one address to another.** The player seeks to the first address before the capture
starts, so the disc is already turning throughout.

**From spin-up to an address.** The front of a whole-side capture: the spin-up, and then as
much of the side as you ask for.

There is no "capture the lead-in" option, because no command puts a player on the lead-in.
The two shapes that hold it get it by starting the capture before the disc.

While it runs you get the stage it is in, the current address, and how long is left. The
estimate is not a guess from the rate so far — a disc plays in real time, so the time
remaining *is* the programme time remaining, and it is right from the first reading.

Other things worth knowing:

- **Lock the player's front panel** is offered, and stops a hand on the player pausing a
  capture halfway through a side. It is released afterwards.
- **The capture name** is prefilled from what the disc turned out to be. If that name is
  already taken you are told before anything is written — and nothing is ever overwritten.
- The disc's facts — model, type, size, side, standard, programme bounds — are written into
  the capture's own metadata, so the file says which side of which disc it is.
- **Stop** finishes the capture properly rather than abandoning it: the file is finalised,
  the player is stopped, and the front panel is released.
- If the **serial cable fails mid-capture, the capture keeps running.** The automation stops
  and says so, and you stop the capture by hand. The player carries on to the end of the
  side regardless, and truncating a good capture because an adapter came loose would be the
  worse of the two failures.

## The coupling, and the direction it does not run in

There is one setting, in player settings and also on the capture setup:

**Stop the capture when the player stops** — **off** by default, deliberately. A player that
briefly reports a stopped transport mid-side would otherwise truncate a good capture. It is
debounced when on, and it is still the setting to think twice about. It sends nothing down
the serial cable: it watches the status the player is already being polled for and stops the
*capture*, which is the application's own to stop.

**A capture stopping never stops the player.** Outside an automatic capture, nothing in this
application sends the player a command you did not ask for — pressing **Stop capture** during
a manual capture leaves the disc exactly where it is, so capturing the first half of a side
and then the second is two presses and no disc movement.

The legacy application had the opposite preference, on by default, and it is deliberately not
carried over. An automatic capture still spins the disc down at the end, because there the
application is the thing operating the player; a manual capture is you operating it.

!!! warning "Nothing ejects a disc by itself"

    On a Pioneer player the stop command is **Reject** (`RJ`), and a Reject arriving while
    the disc is already spinning down opens the tray. An automatic capture therefore sends at
    most one stop per run, and only to a transport it started itself. The tray commands
    (`OP`, `CO`) exist only on the [remote](#the-remote), where a person
    presses them — a machine ejecting a disc unasked, with nobody necessarily in the room, is
    not something this application does.

## Which players have actually been tested

An important distinction, and the application makes it too: the panel says when the
connected model's command set has not been confirmed on real hardware.

| Model | Definition | Confirmed on hardware |
| --- | --- | --- |
| Pioneer LD-V4300D | Yes | **Partly** — this project's own bench player. Connection, disc status, TV system and both user-code reads are measured on it; the full checklist has not yet been walked |
| Pioneer LD-V8000 | Yes | Not yet |
| Pioneer LD-V4400 | Yes | Not yet |
| Pioneer LD-V4200 | Yes | Not yet |
| Pioneer LD-V2200 | Yes | Not yet |
| Pioneer CLD-V2400 | Yes | Not yet |
| Pioneer CLD-V2600 | Yes | Not yet |
| Pioneer CLD-V2800 | Yes | Not yet |
| Pioneer CLD-V5000 | Yes | Not yet |
| Pioneer VC-V330 | Yes | Not yet |
| Anything else answering the Pioneer protocol | Generic | Connects, and is driven with the standard Level III command set |

"Not yet" means the definition is inherited from the documented Pioneer Level III command
set and is a well-founded expectation rather than an observation. Nothing behaves
differently because of it — it is a statement about evidence, and it is the honest answer to
"is my player supported?". A definition that has never met its hardware will be wrong
somewhere, and the places it is wrong are exactly the ones nobody has looked at.

If you have one of these and are willing to walk the checklist, it is in
[`ddd-gui/src/player/players/README.md`](https://github.com/simoninns/DomesdayDuplicator/blob/main/ddd-gui/src/player/players/README.md)
and the full version is in TESTING.md §7. Adding a model is one header file and one line in
a table.

## What it will not do

- **Open the tray as part of an automatic sequence.** Tray control stays a deliberate thing
  you do, never something a capture decides to do.
- **Flip the disc.** Side two is a second capture.
- **Touch anything permanent.** Nothing here writes to any non-volatile memory in the player
  or in the Duplicator. Commands spin discs and move optical assemblies; that is all.
