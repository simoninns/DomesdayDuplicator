# Naming and metadata

Every capture is written with a **metadata file beside it** — same name, `.ddd.yaml` instead
of `.ddd.flac`:

```
Casper_side2_2026-08-17_14-30-00.ddd.flac    the recording
Casper_side2_2026-08-17_14-30-00.ddd.yaml    what it is
```

A FLAC capture already carries its essentials in its own tags, and that stays the thing to
rely on when a file travels alone — see [Capture files](capture-files.md). The metadata file
is the other half: what the person at the bench typed about the disc, what the player was
asked and answered, what an examination of the disc measured, and how the capture itself
went. For an uncompressed `.ddd.s16` capture it is the *only* provenance there is, because
that format has nowhere to put a tag.

## The Naming dialog

The **Naming…** button sits beside the Name field in the [Capture](capture-control.md)
panel. It is a button rather than eight more rows in the panel because that panel shares a
dock column with the scope and the spectrum, and a form long enough to need scrolling would
squeeze them into a strip.

| Field | What it is for |
| --- | --- |
| **Title** | The title on the sleeve |
| **Disc type** | CAV or CLV |
| **Video standard** | NTSC or PAL |
| **Audio** | Default, Analogue, AC3 or DTS |
| **Side** | Which side of the disc this is |
| **Notes** | A short note that can go in the file name |
| **Mint marks** | The condition of the disc, in whatever shorthand you already use |
| **Metadata notes** | A paragraph. Written to the metadata file only, never to a file name |

The same set of fields the [legacy application](../legacy-gui/index.md) collected, so notes
already written against those field names still mean what they meant.

The same fields appear as the first page of an
[automatic capture](player-control.md#capturing-a-side-by-itself), where they arrive
prefilled from the examination. It is the same form in both places rather than two that agree
by inspection, so everything on this page is true of both.

### Ask the player

With a [player connected](player-control.md), **Ask the player** fills in the **disc type**,
the **video standard** and the **side** from the disc itself, and ticks them.

It takes a couple of seconds and **does not move the disc**. It asks the player what it has,
reads the disc's own programme status and its TV system, and stops there — no seeking, no
measuring, and none of the eleven seconds the Pioneer user code costs. It is not the
[examination](player-control.md#examining-a-disc), which does all of that and takes about a
minute.

Two rules about what it touches, and they pull in opposite directions on purpose:

- **Nothing you typed is ever overwritten.** The title, the notes, the mint marks and the
  metadata notes are things only a person knows, and a button that cleared them because a
  disc had been spun up would be unusable.
- **The three fields it can answer are overwritten even if you set them by hand.** Somebody
  who ticked CAV and then asked the disc, which said CLV, asked because they wanted the
  disc's answer.

A field the player could not answer is left exactly as it was, and a side number the form
cannot hold is not followed — a reading outside the range the spin box covers leaves the
field alone rather than clamping it and recording a wrong side as an established fact.

The button is absent when the application has no player layer at all, and disabled when
nothing is connected, with the reason beside it.

### Ticked or not is part of the answer

Each field has a check box carrying its name, and **a field is only recorded when its box is
ticked**. That is a third state rather than a convenience: an empty title and a title nobody
was asked for are different facts, and a spin box showing "side 1" cannot say which of the
two it is. An unticked field is greyed out, does not reach the file name, and is absent from
the metadata file rather than present and blank.

Everything is applied as you type it. There is no OK button, because there is no draft state
to lose — and the file-name preview at the bottom has to track the fields to be worth
showing at all.

### Clear all fields

One press empties every field and unticks every box, ready for the next disc. It is what
makes remembering these fields between sessions safe: without it, the second disc of a
session inherits the first one's title.

Two things it deliberately leaves alone: the two **per-side** options below, because those
are a way of working rather than a fact about a disc.

### Capturing several sides

**Keep separate notes for each side** and **Keep separate mint marks for each side** make
the side number swap the text in and out. Type the notes for side 1, change the side to 2,
and the box is blank; go back to 1 and they are there again.

Held for the session only, and not written to the settings file: text restored a week later
against a different disc would be worse than none.

## How the file name is built

In order of precedence:

1. **[Test mode](test-mode.md)** forces `TestData_<timestamp>`, whatever is set anywhere. A
   file of ramps must never carry a disc's name.
2. **A name typed in the Capture panel's Name field** is used verbatim, with no timestamp.
   The naming fields still reach the metadata file; they simply get no say in what the file
   is called. The dialog's preview says so, so that ticking five boxes and seeing none of
   them in the name is never a mystery.
3. **Otherwise the fields**, followed by the timestamp.

Built from the fields, the name is:

```
<title or RF-Sample>_[CAV|CLV]_[NTSC|PAL]_[ANA|AC3|DTS]_side<n>_<notes>_<mint>_<timestamp>
```

with every part present only when its box is ticked — and the bracketed middle only when
**Include the disc details in the file name** is on. The title and the side are in the name
either way.

The side is in the name whether or not the rest of the details are, and that asymmetry is
deliberate: the two files somebody makes in a row are the two sides of one disc, and telling
them apart afterwards is the whole problem a capture name exists to solve.

`Default` audio is the one choice that means something in the metadata and adds nothing to a
name. It says the disc carries its own default tracks; `_Default` in a file name says less
than the eight characters cost.

### Append the capture's length

`Casper_side2.ddd.flac` becomes `Casper_side2_00H41M12S.ddd.flac`.

The length is not a fact until the capture has stopped, so **the file is renamed at that
point** rather than the length having been guessed at the start. The metadata file is
written beside it under the new name. Letters between the fields rather than colons, because
a colon is not a legal filename character on Windows.

If the rename fails for any reason, the recording keeps the name it already has and the
reason goes in the [Log](main-window.md) panel. A cosmetic disappointment is never allowed
to become a lost session.

## What the metadata file contains

YAML rather than the legacy application's JSON. Both are text and both are parsed by
everything, so the choice is about the reader who is not a program: a sidecar exists to be
legible in five years by somebody with a text editor and no tooling, and YAML lets the
document carry comments explaining its own fields. The structure is deliberately close to
the old application's — a reader written for one is a short edit from the other.

**A field that was never established is absent, not blank.** That is the first rule the
document is built on, and it is why so many of the fields below say "only when". A field
carrying a default nobody checked is indistinguishable from a measurement once the session
is over.

**Every figure in it is about the recording and nothing else** — that is the second. Metadata
is data about the data, so nothing measured over the monitoring session either side of the
file appears here at all. Several figures the [Statistics](statistics.md) panel shows are
therefore absent rather than present with a caveat: ring depth, encoder backlog and the
device's back-pressure peak say how hard the machine was working during a session, which is
worth watching live and is not a property of a recording that outlives that session by years.

Strings are always quoted, even where YAML would accept them bare. A disc title of `no` is
the boolean false in YAML 1.1, `1:30` is a sexagesimal integer, and a leading `*` is an
alias — quoting unconditionally means none of those cases has to be detected, so none of
them can be missed. Bytes that are not printable — which is how a Pioneer user code records
a field that was never encoded — are written as `\x00` escapes rather than dropped.

### Top level

| Key | What it holds |
| --- | --- |
| `schema_version` | `1`. Incremented when a field changes meaning, never when one is added — a reader must keep working against a file with more in it than it knows about |
| `application_version` | The build that produced the capture |
| `capture` | The capture itself |
| `signal` | What the signal looked like — only when there was any |
| `naming` | What you said the disc was |
| `player` | What the player said about itself |
| `disc` | What an examination of the disc found |

### `capture`

| Key | What it holds |
| --- | --- |
| `file` | The capture's file name — the name alone, never the path, so the pair survives being copied to an archive drive |
| `format` | `FLAC` or `signed 16-bit` |
| `test_mode` | Whether this is signal or a test ramp. Always written, either way |
| `sample_rate_hz` | The real rate — `40000000`, or `20000000` when decimating |
| `decimation_factor` | `1` or `2` |
| `front_end_gain` | The declared SW401 position — **only when one was actually declared** |
| `started`, `finished` | ISO 8601, local time with the offset, so the timestamps agree with the file name and are still unambiguous |
| `duration_seconds` | Worked out from the file's own sample count, not from a clock |
| `samples`, `bytes` | What reached the file |
| `completed` | False when the stream ended in a failure rather than because you stopped it. The file is readable either way |
| `detail` | Why, when it did not complete |
| `sequence_check` | `running`, `failed`, `disabled` or `synchronising` — see below |
| `device_overflow_events`, `device_dropped_words` | Samples the device lost inside itself while this file was being written, before this application ever saw them |
| `test_pattern_passed` | Test mode only |

`sequence_check` is the state rather than a yes or no, and that is the point. This is the
integrity claim the whole instrument exists to be able to make, and `disabled` — gateware
that emits no markers — is not "intact", it is "nothing checked". A field that could only
say yes or no would have to lie about one of the four.

### `signal`

`minimum_value`, `maximum_value`, `rms`, `clipped_low_samples`, `clipped_high_samples` —
**measured over this file's own samples and no others.**

The figures the [Statistics](statistics.md) panel shows cover the whole monitoring session,
because that is what is useful while you are watching them, and they are not reset when a
capture starts: doing that would blank the display at the moment you press the button. The
metadata file therefore does not use them. The engine measures a second span that opens when
the file opens and closes when it closes, so a loud minute of setting up before the capture
cannot raise the maximum recorded against the recording.

### `naming`

`title`, `disc_type`, `video_standard`, `audio`, `side`, `notes`, `mint_marks`,
`metadata_notes` — each present only when its box was ticked, and spelled out in full where
the file name gets an abbreviation (`Analogue`, not `ANA`).

### `player`

`model_name`, `model_id_code`, `model_code`, `firmware_version`, `port`, `baud_rate`,
`recognised_model`.

Recorded whenever [player control](player-control.md) is connected, not only for an
automatic capture: a manual capture of a disc in a player is still a capture off that
player. `model_code` is the whole identifying reply, kept because it is what you will be
asked for when a definition needs writing for a player this build does not recognise.
`recognised_model: false` means the player was driven with the generic command set.

### `disc`

Present when an
**[automatic capture](player-control.md#capturing-a-side-by-itself)** ran, because that is the
flow that examines the disc first. A capture taken by hand carries `disc: {}` rather than the
previous disc's answers — the disc in the player is not necessarily the disc that was
examined, and a file asserting otherwise would be worse than one that says nothing.

```yaml
"disc":
  "examined": true
  "disc_type":
    "value": "CLV"
    "source": "reported"
  "programme_end":
    "value": "1:02:03"
    "source": "measured"
```

**Every fact carries how it was established**, and that is the point of recording it at all:
a programme length found by seeking past the end of the side and one the disc merely claims
are both numbers, and a file that showed them alike would have to be believed rather than
read. The four sources are `reported` (the player said so), `measured` (established by
driving the player and reading the result), `inferred` (follows from another fact here) and
`declared` (you said so).

The fields are `disc_present`, `tray`, `disc_type`, `addressing`, `disc_size`, `disc_side`,
`video_standard`, `programme_start`, `programme_end`, `programme_duration`,
`lead_in_reachable` and `chapters`, plus:

| Key | What it holds |
| --- | --- |
| `disc_status_reply` | The player's disc-status reply exactly as it arrived, undecoded. The working, not the answer — a file that says "side 2" and shows the characters it read that from is one you can check |
| `standard_user_code` | `outcome` and `text` |
| `pioneer_user_code` | `outcome` and `text` |

The user codes are recorded as an outcome *and* a value because the outcomes are different
findings: `read`, `not encoded on the disc`, `no usable answer`, and `not read` (never asked
— the Pioneer read is optional, and costs eleven seconds and the player's position). All
four produce nothing to show, and reporting them alike would report the absence of evidence
as evidence of absence.

An examination that established nothing still writes `examined: true`. A player that refused
every query produced a finding, and it is not the same finding as a capture taken with no
examination at all.

## An example

```yaml
# Domesday Duplicator capture metadata.
#
# This file describes the capture of the same name beside it.
# A field that was never established is absent rather than
# blank, so everything written here was actually known.

"schema_version": 1
"application_version": "0.9.1-a1b2c3d"

"capture":
  "file": "Casper_side2_2026-08-17_14-30-00.ddd.flac"
  "format": "FLAC"
  "test_mode": false
  "sample_rate_hz": 40000000
  "decimation_factor": 1
  "front_end_gain": "SW401 4 — 20 dB"
  "started": "2026-08-17T14:30:00+01:00"
  "finished": "2026-08-17T15:11:12+01:00"
  "duration_seconds": 2472.000
  "samples": 98880000000
  "bytes": 59328000000
  "completed": true
  "sequence_check": "running"
  "device_overflow_events": 0
  "device_dropped_words": 0

# Measured over this file's own samples and no others.
"signal":
  "minimum_value": 96
  "maximum_value": 928
  "rms": 271.44
  "clipped_low_samples": 0
  "clipped_high_samples": 0

"naming":
  "title": "Casper"
  "disc_type": "CLV"
  "video_standard": "PAL"
  "side": 2
  "mint_marks": "NM"
  "metadata_notes": "Slight rot at the outer edge of side 2."

"player":
  "model_name": "Pioneer LD-V4300D"
  "model_id_code": "P15"
  "firmware_version": "12"
  "port": "/dev/ttyUSB0"
  "baud_rate": 9600
  "recognised_model": true

"disc":
  "examined": true
  "disc_present":
    "value": "yes"
    "source": "reported"
  "disc_type":
    "value": "CLV"
    "source": "reported"
  "disc_side":
    "value": "2"
    "source": "reported"
  "programme_end":
    "value": "0:41:12"
    "source": "measured"
  "disc_status_reply": "11011"
```

## Reading one

Any YAML library opens it:

```python
import yaml
with open("Casper_side2_2026-08-17_14-30-00.ddd.yaml") as file:
    metadata = yaml.safe_load(file)
print(metadata["capture"]["sample_rate_hz"])
print(metadata["disc"].get("disc_side", {}).get("value"))
```

Use `.get()` for anything under `naming`, `player` and `disc`. Those fields are absent when
nothing established them, which is the ordinary case rather than the exception.

## If the metadata file cannot be written

The capture is finished and complete; only the text file beside it failed. The reason goes
in the [Log](main-window.md) panel and nothing else happens — a message box claiming a
problem would send you looking for a fault in the wrong place.
