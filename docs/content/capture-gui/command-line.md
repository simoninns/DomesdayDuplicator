# Command line

The application is called `ddd-gui`. It is a graphical application and needs no arguments,
but it accepts a few.

```bash
ddd-gui [options]
```

| Where the binary is | |
| --- | --- |
| Linux (Flatpak) | `flatpak run --command=ddd-gui io.github.simoninns.DddGui` |
| macOS | `/Applications/Domesday Duplicator.app/Contents/MacOS/ddd-gui` |
| Windows | `C:\Program Files\Domesday Duplicator\ddd-gui.exe` |

## Options

### `--version`

Prints the commit the binary was built from. This is the string to quote in a bug report.

!!! note "Windows, and where the output goes"

    The Windows build is linked as a GUI-subsystem executable and has no console of its own,
    so at startup it borrows the console it was started from. Output then appears in the
    command prompt as it does everywhere else — but the prompt does not wait for a windowed
    application, so it returns immediately and the output lands underneath it. Started from a
    desktop shortcut there is no console to borrow and nothing is printed at all, which is
    why **Help ▸ About** carries the identical string.

### `--help`

The usual summary of these options.

### `-d`, `--debug`

Log debug-level diagnostics, and show the [Log panel](main-window.md#the-log-panel) at
startup rather than leaving it hidden.

This is what to do before reproducing a fault you intend to report: start with `--debug`,
make the fault happen, then copy the Log panel's contents into the report. Better still,
add [`--log-file`](#-f-log-file-file) and attach the file.

`--debug` is the short way to say `--log-level debug` *and* open the panel. Given both, the
explicit `--log-level` decides the level and `--debug` still opens the panel — so
`--debug --log-level info` is "show me the panel, at the usual level".

### `-l`, `--log-level <level>`

How much is logged, to the [Log panel](main-window.md#the-log-panel) and to the console
alike. One of:

| Level | What it admits |
| --- | --- |
| `trace`, `debug` | Everything, including per-step diagnostics |
| `info` | The default: what the application did |
| `warn`, `warning` | Only what went wrong or nearly did |
| `error`, `critical` | Only failures |
| `off` | Nothing at all |

At `debug`, a capture keeps a full account of itself — see
[What debug level records about a capture](#what-debug-level-records-about-a-capture)
below. That is the level to reproduce a fault at.

The application has four levels of its own, and this vocabulary is the wider one the
project's other tools use, so a level named there means the same thing here: `trace` is a
second name for `debug` and `critical` a second name for `error`. The names are lower case
and an unknown one is an error rather than a silent fall back to `info`.

```bash
ddd-gui --log-level debug
```

### `-f`, `--log-file <file>`

Write the log to a file as well as showing it in the Log panel. The file is **replaced at
every start**, so it describes the run that produced it — reproduce a fault twice and you
have the second attempt, not both.

```bash
ddd-gui --log-level debug --log-file capture.log
```

This is the log to attach to a bug report. The Log panel holds the last few thousand records
and closes with the application; a file survives both. The console stays quiet unless
[`--log-out`](#-log-out-destination) asks for it.

### `--log-out <destination>`

Where the log goes **besides** the Log panel:

| Destination | Where records go |
| --- | --- |
| `none` | The default: the Log panel and nowhere else |
| `console` | The console as well. A `--log-file` is ignored |
| `file` | A log file as well, and nothing to the console |
| `both` | The console and a log file |

The Log panel is not one of these values and cannot be turned off by one. It shows every
record the level admits whatever this says — it is the destination a user who is not looking
at a terminal has, and `none` means *no second copy*, not "no log".

Naming a [`--log-file`](#-f-log-file-file) with no `--log-out` is an explicit request for a
file, so it means `file`. Spelling out `--log-out` always wins over that, including
`--log-out none` beside a `--log-file` — which writes no file, and says so in the panel
rather than leaving a switch that quietly did nothing.

`file` and `both` need `--log-file` to have named one. Asking for either without a file logs
to the panel and the console and says so.

!!! tip "Where the console is"

    Log records go to standard error, so that a script reading `--analyse-test-data`'s
    verdict from standard output does not have to filter them out. On Windows the console is
    the one the application was started from, as described under `--version` above.

### `--analyse-test-data <file>`

Check a test-mode capture for sequence breaks and exit, without opening a window.

```bash
ddd-gui --analyse-test-data TestData_2026-08-16_14-30-00.ddd.flac
```

| Exit code | Meaning |
| --- | --- |
| `0` | The ramp was intact |
| `1` | It broke |
| `2` | The file could not be analysed |

The verdict goes to standard output and "I could not read this" to standard error, so a
script collecting results does not end up with a message about its own arguments in the
collection. On Windows it goes to the console the application was started from, or wherever
a caller redirected it; the exit code works either way, and that is what a script actually
reads.

This is the same analysis **Tools → Test data → Analyse test data…** performs, over the same
code. See [Test mode](test-mode.md).

### `--dev-update-key`

Accept a firmware update file signed with the project's **development** key, whose secret
half is public.

A release build pins the release key and accepts nothing else. This widens that for one run,
and the Update page puts a banner on every development-signed file it then opens. It proves
the file is well formed and **nothing about where it came from**, which is the whole reason
it is opt-in and per-run rather than a setting.

You want this only if you are building update bundles yourself; see
[Developer update loop](../development/developer-update-loop.md).

## What is always logged

Three lines every run, whatever the level, because they are what every fault report is read
against:

```
Capture application started.
Application commit c6ca52b4-dirty.
Platform: NixOS 26.05 (Yarara), kernel linux 6.18.43, x86_64, Qt 6.11.1.
```

The commit is the build; **Help ▸ About** carries the identical string. The platform line
names the operating system as it names itself, the kernel — which is the version that
actually decides how USB behaves, and is the one a kernel-level fault is filed against — the
processor architecture, and the Qt in use. A packaged build that ships one Qt and loads
another says both versions, which is a class of fault that reads as an application bug until
this line contradicts it.

## What debug level records about a capture

`info` says a capture started, stopped, and how it ended. `debug` adds the account a
developer needs to say *why* it ended that way, and none of it needs a rebuild to turn on.

**When the stream starts**, the ring it will run with — asked for, planned, and how much
time it buys before a sample would be lost — and every option the run was made under, so a
log can be read without the settings file that has since been changed.

```
Ring: 256.0 MiB asked for, 127 slots of 2.0 MiB = 254.0 MiB, which is 3.33 s of
  headroom at 40.0 Msps (80.0 MB/s)
Options: test mode off, memory locking on, priority elevation on, stall timeout
  5000 ms, snapshot every 4 buffers of 64.0 KiB, throughput window 1000 ms,
  progress every 10000 ms
```

**While it runs**, a line every ten seconds carrying the throughput against what it should
have been, the ring depth, the device's back pressure, and how much has reached the file.
This is the record of *when* a squeeze happened, which no figure taken at the end can give.

```
Capturing for 4 m 12 s: 9648 buffers, 79.9 MB/s (99.9% of the wire rate), ring
  1/127 slots, peak 6; device buffer 0% back pressure, peak 34%; written
  9.42 GiB (4 m 12 s of stream)
```

**When a capture file closes**, what actually reached it: samples, the length of stream
they are, the size on disk and what that is as a fraction of what arrived — which is the
compression a FLAC capture achieved — and how long the encoder's final flush took, since
that pause is paid for out of the ring's headroom.

```
Closed flac after 9648 buffers over 4 m 12 s: 10113318912 samples = 4 m 12 s of
  stream, 9.42 GiB on disk (46.7% of the 18.83 GiB that arrived), finishing took
  214 ms
Capture file: 10113318912 samples over 4 m 12 s, 9.42 GiB of 18.83 GiB that
  arrived (50.0%), 38.3 MB/s to disk
Signal in the file: range 41-980 of 1023, RMS 331.2, clipped low 0 high 0
While this file was open: device lost 0 samples in 0 overflows; session peak back
  pressure 34%, session peak ring depth 6 of 127 slots
```

**When the stream stops**, the whole run: what went through, how full each buffer got, and
what the device's own FIFO did. Back pressure is reported as a mean and as counts at three
levels beside the peak, because a run that touched three quarters of the ring once and one
that sat there for twenty minutes report the same peak and are not the same capture.

```
Run: 4 m 20 s, 9962 transfers, 9962 buffers = 19.45 GiB = 4 m 20 s of stream,
  averaging 79.8 MB/s (99.8% of the wire rate, counting the discarded opening
  slots and anything still in flight)
Ring depth: mean 0.9%, peak 4.7% (9962 readings), never over a quarter full; peak
  6 of 127 slots, 9962 filled, 9962 freed
Device back pressure: mean 2.1%, peak 34% (1043 readings), over a quarter for 3,
  never over half; at or above the gateware's near-full mark for 1.42 s, peak
  9216 words
Signal over the run: 10402320384 samples, range 38-988 of 1023, RMS 330.9,
  clipped low 0 high 0; sequence intact
```

Two of those figures are worth knowing how to read:

- **Ring depth** is the host keeping up. A healthy capture sits near zero and never
  climbs; a mean that is not near zero means this machine is only just managing, whatever
  the peak says.
- **Device back pressure** is the device waiting for the host to take packets, on a scale
  where zero means every packet was taken as soon as it was offered and one hundred means
  samples were lost. The near-full time beside it is how long the device spent close to
  the edge, which is the figure that survives a squeeze nobody was reading at the moment
  it happened.

## What debug level records about an update or a bring-up

Installing firmware and bringing a board up are the two operations that touch a device
permanently, and both are minutes long. At `debug` each keeps its own account, and the
engine's half and the interface's half go to the same place.

**Choosing a file** — what it is, whether it verified, and what the compatibility gate made
of it beside the device it was asked about. A file that verifies and cannot be installed is
the commonest thing to be handed, and the verdict alone does not say why:

```
Update page: opening /home/me/Downloads/domesday-duplicator-update-1.4.0.dddfw
Update file verified: version 1.4.0, release channel, 3.41 MiB, …
Compatibility gate: allowed (device attached, personality application, firmware
  protocol 2, register map 3)
```

**Installing** — what the file carries, what is being written, the shape the device
negotiated, and how long each part took:

```
Update starting: bundle version 1.4.0, channel release, restart deferred no
  carries firmware 4d23a25, 195.3 KiB, sha256 c7a7d73b, interface version 2
  carries gateware 4d23a25, 1.02 MiB, sha256 91b0e47c, interface version 3
Installing firmware 4d23a25, 195.3 KiB, …: 98 chunks of 2.0 KiB (the device
  offered 2048 bytes), device phase idle
Sent the whole firmware: 98 chunks, 195.3 KiB in 12.41 s (16.1 KiB/s). Waiting for
  the device to finish writing and reading back.
Installed firmware after 41.20 s
Update finished after 3 m 04 s: succeeded at stage Complete, identity confirmed,
  device reports "Domesday Duplicator (4d23a25)", gateware 4d23a25
```

**When it fails** — the device's own account of itself at the moment it stopped, which is
the diagnosis and the one thing nobody can go back and ask for afterwards:

```
Failed to install firmware after 2.31 s: The device could not write to its own
  memory. The update was not completed.
Device status at the failure: phase failed, error write (10), 200000 received,
  0 written, 0 verified
Update finished after 2.34 s: failed at stage Failed, identity not confirmed; …
```

**A bring-up** records the same for its writes, and around them the procedure itself: every
page the wizard visited, what the USB-Blaster probe found, whether the file carries all four
payloads a bring-up needs, each step and how long it took, and — because the JTAG step is
allowed a second attempt — what each attempt did:

```
Bring-up: moved to page 5 of 9 — Load the gateware
Bring-up: USB-Blaster probe: opened
Bring-up: update file verified: version 1.4.0, release channel, carries firmware
  yes, factory image yes, gateware yes, JTAG vectors yes
Bring-up: starting the gateware-over-JTAG step
Configuring the FPGA over JTAG: 1.38 MiB of vectors from the update file, up to 2
  attempts. Nothing is written to the board by this step.
JTAG attempt 1: succeeded after 2.71 s, cable opened, 1284 statements, 5717248
  bits shifted
Bring-up: the gateware-over-JTAG step succeeded after 2.83 s
Programming the board, in the one order that leaves every interruption
  recoverable: EEPROM (195.3 KiB), factory image (1.02 MiB), then application
  image (1.02 MiB). Nothing is restarted here.
```

and, at the end, the last page's verdict check by check — a board that fails one of them is
a completely different problem from one that fails all of them.

## A related tool

`ddd-update` installs a firmware bundle from a shell, over the identical engine code the
application's Update page uses. It links no Qt at all. It is built alongside the
application from source but is not part of the released packages — it is a developer tool,
and [Developer update loop](../development/developer-update-loop.md) is its page.
