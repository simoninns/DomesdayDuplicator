# Scripting captures

The application takes a capture from a script as well as from its window: the same code, the
same file, the same metadata beside it. What the command line adds is a way to *start* a
capture at a moment your script chooses, and a way to stop it again from somewhere else.

That is mostly wanted for one thing. If you are recording the analogue audio of a disc with
another tool while the Duplicator takes the RF, the two recordings have to begin close
together, and neither of them begins when a person presses a button. A script starts the RF
capture, waits until it is really running, starts the audio, and stops both at the end.

The other cases are unattended ones: a fixed-length capture with nobody in the room, a
machine with no display, a rig that takes a side and moves on.

## Three ways to run it

| | What it does |
| --- | --- |
| `--start-capture` | Opens the window as usual and starts capturing as soon as a device is found. You watch it and stop it however you like |
| `--start-capture --headless` | No window at all. Runs until the duration limit, until it is stopped, or until it is interrupted, then prints the finished file and exits |
| `--stop-capture` | Not a capture at all: a message to an application that is already running. Stops its capture, waits for the file to be finished, prints it, exits |

Given on their own, the attribute options — the folder, the name, the rate, the limit, the
format — start nothing. They fill the window in, which is the "set this up for me and I will
press the button myself" case.

| Where the binary is | |
| --- | --- |
| Linux (Flatpak) | `flatpak run --command=ddd-gui io.github.simoninns.DddGui` |
| macOS | `/Applications/Domesday Duplicator.app/Contents/MacOS/ddd-gui` |
| Windows | `C:\Program Files\Domesday Duplicator\ddd-gui.exe` |

Both packaged platforms need a little more than the path, and neither is obvious from
outside: see [Running it from a Flatpak](#running-it-from-a-flatpak) and
[Running it on Windows](#running-it-on-windows) below.

## The options

| Option | What it takes | What it does |
| --- | --- | --- |
| `--start-capture` | | Start capturing once a device is found |
| `--headless` | | No window. Needs `--start-capture` |
| `--stop-capture` | | Stop the capture a running instance is taking. Cannot be combined with any of the others |
| `--capture-directory <folder>` | a folder | Write here instead of the configured folder. Created if it is not there |
| `--capture-name <name>` | a name | Call the capture this, without a suffix |
| `--sample-rate <msps>` | `40` or `20` | Capture at this rate. The decimation is done by the device |
| `--duration-limit <seconds>` | 1 to 86400 | Stop by itself after this long. Leave it out to capture until stopped |
| `--output-format <format>` | `flac` or `s16` | Write [FLAC, or uncompressed `.ddd.s16`](capture-files.md) |

Anything the command line does not mention is left exactly as
[Settings](settings.md) has it, so a script says what is different about *this* capture and
nothing else.

### What the command line sets, it does not save

Values given on the command line apply to that run and are then forgotten. Nothing is
written to your settings, so a scripted capture at 20 Msps into a scratch folder does not
quietly become what the window opens showing tomorrow.

!!! note "The one way that can bite you"

    Session-only means *the command line never saves anything*. It does not mean the values
    are kept apart from your settings while the application runs — the window is showing
    them because they really are the settings this run is using.

    So if you start with `--start-capture --sample-rate 20` and then change something else
    in the window, what gets saved is what the window is showing, 20 Msps included. That is
    only reachable with a window in front of you; a `--headless` run saves nothing whatever
    happens.

### The folder and the name

`--capture-directory` is created if it does not exist, one level or several, exactly as a
capture started from the window creates it. A path that exists and is not a folder is
refused before anything opens.

`--capture-name` is a name and not a path — a `/` or a `\` in it is refused rather than
accepted and then failing much later, when the file is opened. Name the folder with
`--capture-directory`.

The name you give is a starting point, not the final file name. The
[naming rules in Settings](capture-naming.md#how-the-file-name-is-built) still apply, so the
file may end up with [its length appended](capture-naming.md#append-the-captures-length), or
[a number added](capture-naming.md#if-the-name-is-already-there) if the name was taken. This
is exactly why the finished path is printed at the end: **read the path, do not construct
it.**

A script that runs repeatedly with the same `--capture-name` therefore accumulates files
rather than replacing them — `disc-42_00H30M00S`, then `disc-42_00H30M00S (1)`, and so on,
with the new name reported on standard error each time. Nothing a capture writes is ever
written over. If you want one file per run rather than a growing pile, give each run a name
of its own, or leave `--capture-name` out and take the generated name, which carries a
timestamp and is unique by construction.

### The duration limit

In seconds, from 1 to 86400 (24 hours). Zero is refused rather than taken to mean *no
limit* — a script that worked out a limit of zero has a bug in it, and capturing until
something else intervened would hide that bug rather than report it.

The window's own field is in whole minutes, so with `--start-capture` and no `--headless` a
limit that is not a whole number of minutes cannot be shown exactly. The run still uses the
seconds you gave; it is only the displayed value that rounds.

## What a run prints

**Standard output carries the path of the finished capture, alone, and nothing else.**
Everything meant for a person watching — what it is capturing to, that it stopped, how many
bytes were written, anything that went wrong — goes to standard error.

That split is the whole interface. It means a script can do this

```bash
file=$(ddd-gui --headless --start-capture --duration-limit 60)
```

and have the path, with no parsing, no filtering and nothing to go wrong when a capture is
named after a disc with a comma in its title.

The application's own log is separate again, and a headless run is quiet by default. Add
[`--log-out console`](command-line.md#-log-out-destination) to see it, or
[`--log-file`](command-line.md#-f-log-file-file) to keep it — that is what to do before
reporting a fault in a scripted capture.

### What a script may rely on

Three things, and they are meant to be depended on:

- the finished path on **standard output**, on its own line;
- the **exit code**, from the table below;
- the line **`Capturing to <path>`** on standard error, which is printed the moment the file
  is open and is how a script knows the capture is really running.

The rest of what goes to standard error is written for a person and may be reworded.

!!! warning "Use that line as a starting gun, not as a file name"

    The path in `Capturing to` is the file *while it is being written*, and it is often not
    what the capture ends up called: a name with
    [its length appended](capture-naming.md#append-the-captures-length) is only given that
    length once the capture has one, so `disc-42.ddd.flac` becomes
    `disc-42_00H41M12S.ddd.flac` at the end.

    The path on standard output is the finished file, and it is the one to keep.

## Exit codes

| Code | What happened | What to do about it |
| --- | --- | --- |
| `0` | The capture ran and the file was finished | Read the path on standard output |
| `1` | The command line could not be made sense of | The message on standard error names the option |
| `2` | Another Domesday Duplicator is already running, or the control socket could not be created | Stop the other one, or wait for it |
| `3` | No device was found | Nothing was captured. Check the cable and the [device state](if-a-capture-fails.md) |
| `4` | A capture started and then something went wrong | A file exists. Go and look at it — it is as complete as it could be made |
| `5` | `--stop-capture` found nothing running to stop | The capture had already stopped, or was never started |

Exit code `3` is reported after a wait of about ten seconds, which is enough for a device
that is being plugged in as the script starts. A windowed `--start-capture` has no such
limit — the window is open and can wait indefinitely, because somebody is looking at it.

## Only one at a time

Two processes cannot stream from one Duplicator, so a second `--start-capture` or
`--headless` run refuses with exit code `2` rather than racing the first one for the device.
An application that was killed rather than stopped leaves nothing in the way: the socket it
left behind is recognised as abandoned and cleaned up by the next run.

The exclusion is per user, and it does not reach across packaging: a Flatpak instance and a
locally built binary do not know about each other, and neither can stop the other.

## Stopping a capture

Every way of stopping ends the same: the stream is detached, the encoder finishes the file,
the [duration is appended](capture-naming.md#append-the-captures-length) if the naming asks
for it, and the [metadata file](capture-naming.md#what-the-metadata-file-contains) is written
beside it. Only then does the process exit and print the path.

| How | Where it works | When to use it |
| --- | --- | --- |
| `--duration-limit` | Everywhere | The length is known in advance |
| `ddd-gui --stop-capture` | Everywhere | Another script, another terminal, another machine's session — anything that is not the capture's own console |
| Ctrl+C | Everywhere | You are sitting in front of it |
| `kill` (`SIGINT` or `SIGTERM`) | Linux, macOS | A script that holds the process ID |
| `CTRL_BREAK_EVENT` | Windows | A script that holds the process ID — see [below](#running-it-on-windows) |

`--stop-capture` waits for the file. It does not return when the capture stops; it returns
when the capture has been *finished*, and prints the same path the capturing process prints.
A script can stop a capture and read the file on the next line with nothing in between.

!!! warning "Never `kill -9` a capture"

    `SIGKILL` cannot be caught, so nothing runs: the FLAC stream is left without its final
    block, no metadata file is written, and what is on disk may not open. Every other way of
    stopping finishes the file properly. If a capture appears stuck, `--stop-capture` and
    wait — finishing a large file on a slow disk takes as long as it takes.

Interrupting twice does no harm and does not make it quicker. Once the file is being
finished, a second interrupt is answered with a line saying as much and otherwise ignored —
abandoning the file at that point is the one thing that would leave it unreadable.

## Worked examples

### Audio and RF, started together

The case this was built for. The RF capture is the one with a device to open and buffers to
fill, so it goes first; the audio starts when the RF capture says it is running.

```bash
#!/usr/bin/env bash
set -euo pipefail

directory=~/captures/disc-42
name=disc-42-side-1
mkdir -p "$directory"

# Start the RF capture. Its path will land in $directory/rf.path when it finishes.
ddd-gui --headless --start-capture \
        --capture-directory "$directory" --capture-name "$name" \
        > "$directory/rf.path" 2> "$directory/rf.log" &
rf=$!

# Wait until it is really capturing, rather than guessing with a sleep.
until grep -q '^Capturing to ' "$directory/rf.log"; do
    if ! kill -0 "$rf" 2>/dev/null; then
        echo "The RF capture did not start:" >&2
        cat "$directory/rf.log" >&2
        exit 1
    fi
    sleep 0.1
done

# Now the audio, with whatever records it on this machine.
rec -q -c 2 -r 48000 -b 24 "$directory/$name.wav" &
audio=$!

read -r -p "Press return when the side has played out... "

kill -INT "$audio"
ddd-gui --stop-capture > /dev/null
wait "$rf"

echo "RF:    $(cat "$directory/rf.path")"
echo "Audio: $directory/$name.wav"
```

### A fixed-length capture, unattended

Nothing to stop it and nobody watching. The exit code is the whole report.

```bash
#!/usr/bin/env bash
set -euo pipefail

# Taken from the command itself rather than read back after an `if`, where the
# negation would have replaced it with its own success.
status=0
file=$(ddd-gui --headless --start-capture \
               --capture-directory /captures/disc-42 \
               --capture-name disc-42-side-1 \
               --sample-rate 20 \
               --duration-limit 1800) || status=$?

if (( status != 0 )); then
    case $status in
        2) echo "Something else is using the Duplicator." >&2 ;;
        3) echo "No Duplicator was found." >&2 ;;
        4) echo "The capture failed part way through." >&2 ;;
        *) echo "The capture could not be started." >&2 ;;
    esac
    exit "$status"
fi

echo "Captured $file"
ls -lh "$file" "${file%.ddd.flac}.ddd.yaml"
```

### Stopping one from somewhere else

Any shell, any script, at any point:

```bash
ddd-gui --stop-capture
```

It works whether the capture was started from a script or from the window, because every
instance listens for it.

## Running it from a Flatpak

Options go after the application ID, and everything above applies unchanged:

```bash
flatpak run --command=ddd-gui io.github.simoninns.DddGui \
    --headless --start-capture \
    --capture-directory ~/captures/disc-42 --capture-name disc-42-side-1 \
    --duration-limit 1800
```

Stopping it is the same command with `--stop-capture`, and it reaches the running instance
because both are the same application:

```bash
flatpak run --command=ddd-gui io.github.simoninns.DddGui --stop-capture
```

Two things about a sandbox are worth knowing before you write the script rather than
afterwards.

!!! warning "`kill` does not reach inside the sandbox"

    A Flatpak application runs inside its own process namespace, and `flatpak run` is not
    it. Sending `SIGTERM` to the `flatpak run` process ends the sandbox and takes the
    capture down with it, mid-file — the application never sees the signal and never
    finishes the file.

    Ctrl+C at a terminal *does* work, because that reaches every process in the group. But
    a script must stop a Flatpak capture with `--stop-capture`, not with `kill`.

!!! warning "`/tmp` is not the `/tmp` you can see"

    A sandbox has a private `/tmp` of its own, held in memory and invisible from outside it.
    `--capture-directory /tmp/something` is accepted — the folder does exist, from inside —
    and then fills your RAM with capture data that no other program can read and that
    disappears when the application exits.

    Captures belong under your home directory, or on a drive under `/run/media`, `/media` or
    `/mnt`. Those are the paths the sandbox is given, and they mean the same thing inside it
    as out.

## Running it on Windows

Two things are different, and both come from the same cause: the application is a windowed
program, so a command prompt does not wait for it.

**Wait for it explicitly, or you get no exit code.** `ddd-gui.exe` returns to the prompt
immediately and `%ERRORLEVEL%` means nothing. In PowerShell, which is the easier of the two:

```powershell
$ddd = "C:\Program Files\Domesday Duplicator\ddd-gui.exe"

$run = Start-Process -FilePath $ddd -Wait -PassThru -NoNewWindow `
    -ArgumentList '--headless','--start-capture',
                  '--capture-directory','D:\captures\disc-42',
                  '--capture-name','disc-42-side-1',
                  '--duration-limit','1800' `
    -RedirectStandardOutput 'D:\captures\disc-42\rf.path'

if ($run.ExitCode -ne 0) { throw "The capture failed with $($run.ExitCode)" }
Get-Content 'D:\captures\disc-42\rf.path'
```

From `cmd.exe`, `start /wait /b` waits and keeps the current console, so a redirection still
lands where you put it:

```bat
set DDD="C:\Program Files\Domesday Duplicator\ddd-gui.exe"
start /wait /b "" %DDD% --headless --start-capture ^
    --capture-directory D:\captures\disc-42 --capture-name disc-42-side-1 ^
    --duration-limit 1800 > D:\captures\disc-42\rf.path
echo Exit code %ERRORLEVEL%
```

**The installer does not put it on your `PATH`**, so scripts name it in full. It is
`C:\Program Files\Domesday Duplicator\ddd-gui.exe` unless you chose a different folder when
installing.

Stopping works as it does everywhere: `--stop-capture` from another prompt, or Ctrl+C in the
console the capture is running in, both finish the file properly. A script that holds the
process ID can send `CTRL_BREAK_EVENT` with `GenerateConsoleCtrlEvent`, which is handled the
same way. Closing the console window is the one exception — Windows allows a few seconds and
then ends the process regardless of what it is doing, so a capture stopped that way may be
left unfinished. Use `--stop-capture`.

## Running it on macOS

The binary is inside the application bundle:

```bash
/Applications/Domesday\ Duplicator.app/Contents/MacOS/ddd-gui --stop-capture
```

Everything else is as it is on Linux, signals included. Both commands have to run as the
same user — the control socket is private to the account that created it.
