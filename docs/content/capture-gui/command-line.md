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

!!! warning "Not on Windows"

    The Windows build is linked as a GUI-subsystem executable, so it has no console attached
    and `--version` writes to one that is not there. **Use Help ▸ About instead**, which
    carries the identical string. That is why the dialog exists rather than being a
    duplicate of this option.

### `--help`

The usual summary of these options.

### `-d`, `--debug`

Log debug-level diagnostics, and show the [Log panel](main-window.md#the-log-panel) at
startup rather than leaving it hidden.

This is what to do before reproducing a fault you intend to report: start with `--debug`,
make the fault happen, then copy the Log panel's contents into the report.

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
collection. On Windows the output goes wherever a caller redirects it, and the exit code
works either way — which is what a script actually reads.

This is the same analysis **Tools → Analyse test data…** performs, over the same code. See
[Test mode](test-mode.md).

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
