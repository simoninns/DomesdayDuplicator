# ddd-gui — capture application

The capture and signal-monitoring application for the Domesday Duplicator.

This is the application being built to replace [gui/](../gui/). Both are in the tree and
both are built by CI: `gui/` is the one that captures today, and it stays until this one
has passed the hardware capture-integrity procedure in [TESTING.md](../TESTING.md) §5.
Nothing here is a supported capture path yet.

The phased plan it is being built to is
[docs-tech/ddd-gui-implementation-plan.md](../docs-tech/ddd-gui-implementation-plan.md).

## What works today

**The application shell**: a themed, panelled window with light and dark themes, dockable
panels that can be floated into windows of their own, a View menu built from the panels
themselves, persistent window and layout state, and a log panel fed through the engine's
logging seam.

**The capture engine**, complete except for the device. The ring buffer, the sequence
validator, the metrics, the monitor tap, the FLAC writer and reader, and the orchestrator
that runs them all on their own threads are here and tested — driven by a synthetic source
that generates the device's stream in software at its real 80 MB/s.

**There is no device code yet**, and the panels are still placeholders that say so. The
GUI does not start the engine; the engine is exercised only by its tests.

That the engine can be tested at all without hardware is the point of the split. The old
application could only be proven by attaching a device and hoping a fault reproduced;
here, a sequence break, a short transfer, a stalled device and a buffer overflow can each
be *asked for* and the response checked on every push. It does not replace the hardware
procedure in [TESTING.md](../TESTING.md) §5 — everything past the host's memory is still
out of reach — but it means a hardware session is spent on hardware questions.

## Building

Requires **Qt 6.5 or later**, CMake 3.21+, and GoogleTest. Qt 6.5 is the floor because
`QStyleHints::colorScheme()` — how the application follows the desktop's light/dark
setting — arrived in that release.

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

**libFLAC** is needed too — 1.4.0 or later, and 1.5.0 or later to encode on more than one
core. It is BSD-3-Clause, so linking it into a GPLv3 application is fine.

libusb is still absent, and deliberately: it arrives with the device backends, and until
then requiring it would mean a build that needs a library nothing links against.

With Nix, from anywhere in the working tree:

```bash
nix develop .#ddd-gui     # dev shell, including the clang tooling the gates need
nix build .#ddd-gui       # the package, tests included
```

### Quality gates

Both run as part of an ordinary build, and both fail it:

- **clang-format** (`--dry-run --Werror`, `BasedOnStyle: Google`) over every source file,
  one stamp per file so a failure names the file.
- **clang-tidy** (`google-*`, `bugprone-*`, warnings as errors) on every target.

They are turned off in the Nix package build, and only there: that sandbox has an
unrelated clang-format version whose output differs, and an unwrapped clang-tidy that
cannot resolve the standard library headers. Neither failure would say anything about the
code. The native CI build is where the gates run.

To disable them locally — for a bisect, or on a machine without the tools:

```bash
cmake -B build -S . -DDDD_ENABLE_CLANG_FORMAT=OFF -DDDD_ENABLE_CLANG_TIDY=OFF
```

## Layout

```
src/capture/      ddd::capture — the engine. Qt-free, by rule.
src/gui/          ddd::gui — the Qt layer, built as a static library, plus main().
cmake/            FindFLAC.cmake, a component-local copy (AGENTS.md §2)
tests/unit/       T1, engine. Links no Qt at all.
tests/golden/     T2, the capture file format checked against what it must be on disk.
tests/functional/ T1, the whole pipeline at the device's rate. Minutes, not seconds.
tests/gui/unit/   T1, Qt layer. Runs under a QCoreApplication; no display needed.
tests/support/    Fixtures shared between test binaries.
```

The functional tier is not part of the ordinary edit-build-test loop. `ctest -L unit`
skips it; a bare `ctest` runs it, and so does CI. Each soak runs for a minute by default —
`DDD_SOAK_SECONDS` shortens that for a quick local check or lengthens it for an overnight
run, and whichever it ends up as is printed, so a shortened run cannot be mistaken for a
full one.

The Qt-free rule on `src/capture/` is load-bearing rather than tidy. Everything a capture
needs — device discovery, the transfer pipeline, validation, writers — belongs there, so
that it can be tested without a display and so that the planned headless command-line
capture tool can reuse it unchanged. `ddd_capture_tests` links no Qt, which is what makes
the rule enforceable: if the engine ever grows a Qt dependency, that binary stops linking.

## Versioning

`--version` reports the commit the binary was built from. Builds outside a git checkout
must pass it, since there is no `.git` for CMake's fallback to consult:

```bash
cmake -B build -S . -DDDD_VERSION=abcd1234
```

A release artefact reporting `unknown` fails the release gate ([AGENTS.md](../AGENTS.md)
§9).

**Help → About shows the same string**, and that is deliberate rather than duplication.
The Windows build is linked as a GUI subsystem executable, so `--version` writes to a
console that is not attached and the user sees nothing at all; on the platform with the
most installations, the dialog is the only way to identify a binary. A user reporting a
bad capture has to be able to say which build produced it. The About text is built by a
pure function (`about_text.h`) so a test can assert it carries the version — a modal
dialog cannot be checked any other way. The CI `--version` check therefore runs on Linux
and macOS only; the About test covers all three.

## Conventions

Google C++ style, enforced by the gates above; SPDX headers on every file
([AGENTS.md](../AGENTS.md) §5.4); `snake_case` filenames, `PascalCase` types and methods,
`trailing_underscore_` members, `kConstants`. Every public class documents its
thread-safety contract in its header comment — with a real-time capture pipeline arriving
in later phases, "which thread may call this" is not something to leave to inference.
