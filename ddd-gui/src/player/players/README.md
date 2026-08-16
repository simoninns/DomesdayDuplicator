# Adding a player

A player model is a header in this directory and a line in
[`../player_registry.cpp`](../player_registry.cpp). Nothing else in the application needs
to change — no transport code, no interface code, no switch statement. If a model cannot
be described without editing something else, that is a gap in the schema
([`../player_definition.h`](../player_definition.h)) and worth fixing there rather than
working around here.

## The steps

1. **Find out what the player answers to the model request.** Connect it, enable player
   control, and read the connection state: an unrecognised player still connects, using
   the generic Pioneer command set, and the application shows the model code it reported —
   `P1506A9` is the prefix `P15`, the model ID `06` and the firmware revision `A9`. The
   model ID is what your definition claims.

2. **Copy the nearest existing header.** For a Pioneer, that is any of them;
   [`pioneer_ld_v4300d.h`](pioneer_ld_v4300d.h) is the shortest.

3. **Set the identity** — `name` as a user should see it, `id_code`, and (for a player
   from a new manufacturer) `manufacturer`.

4. **Adjust the capabilities.** Every flag in `PlayerCapabilities` defaults to true,
   because the Pioneer Level III set is the baseline and most models have all of it. Turn
   off what this model does not have. A capability you leave on must have a command to
   send for it: the registry `static_assert`s that, so a claim with nothing behind it
   fails the build rather than reaching a user as a button that does nothing.

5. **Override only the commands that differ.** [`pioneer_ld_v8000.h`](pioneer_ld_v8000.h)
   is the worked example — one extra command, and a capability gated on the firmware
   revision rather than on the model.

6. **Add it to the registry.** One entry in `kPlayers`, one `static_assert(IsConsistent(…))`
   beside the others, and one row in the expectation table in
   `tests/player/test_player_registry.cpp`.

7. **Run the unit tests.** `ctest --test-dir build -L unit`. The registry sweep will
   object to a duplicate model ID, a missing command, an argument width that cannot be
   encoded, or a command too long for a player to accept.

8. **Walk the bench checklist below**, and only then set `bench_verified = true`.

## What a new family needs

A player that does not answer the Pioneer model request needs its own `ProbeSpec` — the
request, the shape of the reply, where the model ID and firmware revision sit in it, and
the baud rates to try — added to `kProbes` in the registry. The session iterates the
distinct probes and has no idea how many families there are, so nothing else changes.

## `bench_verified`

A definition inherited from the Level III base is a plausible guess until somebody points
it at the hardware. `bench_verified` records which of those two a definition is, and the
application says so: a user driving an unverified definition is told that the command set
has not been confirmed on their model. It is not a claim about capability — nothing in
the library behaves differently — it is a claim about evidence, and it is the honest
answer to "is my player supported?".

**Do not set it because the tests pass.** The tests prove the definition is internally
consistent and encodes the bytes it says it does. They cannot prove those are the right
bytes for a player none of them has ever met.

## The bench checklist

With the player attached, per model. This is the T5 procedure; TESTING.md carries the
full version and this is its summary.

- Connect with auto-detection, from a configuration that has never seen this player.
  Confirm the model name and firmware revision shown are right.
- Repeat at each baud rate the player's own switches offer, fixing the rate in the
  settings each time and then letting auto-detection find it.
- Every control in the remote, one at a time, watching the player rather than the
  application. A control that is enabled and does nothing is a wrong capability flag; a
  control that does something other than its label is a wrong command sequence. In order,
  because each one leaves the player somewhere the next needs it: tray open and close, play,
  pause, still, step both ways, scan both ways, multi-speed both ways at each rate the
  selector offers, a frame search and a time search, a chapter search, the on-screen display
  on and off, each audio mode, key lock on and off, and reject.
- Any control the remote leaves greyed out, checked against the model's manual. A control
  that is disabled and the player does have is a capability flag that is wrong in the other
  direction, and it is the one this checklist would otherwise never reach.
- Both user-code reads, with a disc **spinning**. `E04` from either is an error code and not
  a code to record: the LD-V4400 manual documents it as no user code being encoded, and on
  this project's bench a disc that reads perfectly while spinning gives `E04` parked. The
  remote says "the player refused" rather than showing it as the disc's user code.
- **Do the Pioneer read last, or expect to lose your place.** `?U` is not a query: the player
  searches to the lead-in to answer it. On an LD-V4300D that took 11.1 seconds from the
  middle of a side and left the player parked at frame 1. If a model takes materially longer
  than the thirty seconds the long timeout allows, its definition needs its own.
  In the dump, a run of `` ` `` is the *player* reporting characters it could not read off
  the disc and a run of NULs is data that was never encoded — both are facts about that disc
  rather than faults in the definition, and they are not the same fact.
- The manual command field, with `?X`. Its answer is the model code that belongs in this
  header, so it is also the check that the definition is claiming the right ID.
- Examine a CAV disc and a CLV disc. The reported type, length and addressing must match
  the disc.
- Power-cycle the player mid-session and confirm it reconnects on its own.
- Unplug the serial adapter mid-session and confirm the application reports it once and
  does not spin.

Anything that fails is a definition change, not an application change — which is the
whole point of the schema.
