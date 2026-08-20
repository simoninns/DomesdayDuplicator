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
and closes with the application; a file survives both.

### `--log-out <destination>`

Where the log goes, for anyone who wants one destination and not the other:

| Destination | Where records go |
| --- | --- |
| `console` | The console only. A `--log-file` is ignored |
| `file` | The log file only, and nothing to the console |
| `both` | The default: the console, plus the log file when one was named |

`file` and `both` need `--log-file` to have named a file. Asking for either without one logs
to the console and says so, rather than quietly discarding the log.

The Log panel is not one of these. It shows every record the level admits whatever this is
set to, because it is the destination a user who is not looking at a terminal has.

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

## A related tool

`ddd-update` installs a firmware bundle from a shell, over the identical engine code the
application's Update page uses. It links no Qt at all. It is built alongside the
application from source but is not part of the released packages — it is a developer tool,
and [Developer update loop](../development/developer-update-loop.md) is its page.
