# AGENTS.md and TESTING.md

Plan for adding the two project-convention documents, modelled on
[decode-orc](https://github.com/decode-orc/decode-orc)
(`/home/sdi/Coding/decode-orc/AGENTS.md`, 566 lines; `TESTING.md`, 363 lines) but adapted to
a project that spans host software, bare-metal firmware, FPGA gateware, a PCB and a website.

## Non-negotiable rules — these go at the very top of AGENTS.md

Two rules matter more than anything else in the document and must appear **first**, before
any architecture or testing guidance, stated as absolutes rather than preferences.

### Rule 1 — No automatic git operations

An agent must **never** run a command that changes repository state unless the user has
explicitly asked for that specific action in that specific message:

> **Forbidden without an explicit request:** `git add`, `git commit`, `git push`,
> `git stash`, `git rebase`, `git reset`, `git revert`, `git merge`, `git cherry-pick`,
> `git tag`, `git branch -d`, `git checkout`/`git switch` that discards changes, and any
> `gh pr create`/`gh pr merge`.
>
> **Always permitted:** `git status`, `git log`, `git diff`, `git show`, `git blame`,
> `git ls-files` — anything read-only.

Clarifications that prevent the usual loopholes:

- "Make this change" means edit the files. It does **not** imply staging or committing.
- Permission granted for one commit does not carry to the next one.
- Finishing a task is not a trigger to commit. Leave the work in the working tree and say
  what changed.
- If a commit genuinely seems useful, **ask** — do not commit and offer to undo it.

This matters more here than in a typical repository: the reorganisation involves history
rewrites, unrelated-history merges and a force-push
([submodule-migration.md](submodule-migration.md)), where an unrequested `git commit` or
`git push` at the wrong moment is expensive to unpick.

### Rule 2 — No AI attribution in commits, PRs, code or documentation

Nothing the project produces may advertise the tools used to produce it. **No co-author
trailers, no generation notices, no tool branding**, anywhere:

> **Forbidden strings** — in commit messages, PR titles and bodies, code comments,
> documentation, changelogs and release notes:
> - `Co-Authored-By: Claude …` or any `Co-authored-by:` naming an AI tool or service
> - `Generated with …`, `Created by …`, `Written with the help of …`
> - `🤖`, "AI-assisted", or any tool/vendor name used as an attribution
> - Links to AI products in generated content

Commit messages describe **the change**, not how it was written. This is a house style
decision, and it is also why it must be explicit: several coding assistants append such
trailers by default unless told not to, so silence in AGENTS.md is read as consent.

The same restriction covers advertisements, promotions and commercial references generally —
this project is a preservation tool, not a shop window.

Both rules are carried over from decode-orc's AGENTS.md §1.2 and §1.4, hardened with the
explicit command list and forbidden-string list above, because the abbreviated form has
proven easy to interpret loosely.

## 1. Starting point: there are no tests

Verified across the whole tree: **no `enable_testing()`, no `add_test()`, no GoogleTest,
Catch2 or QTest, anywhere.** The two things that look like tests are not:

- `gui-app/tools/dddutil/analysetestdata.cpp` — a *product feature* that analyses captured
  test-pattern data (see §4, it becomes the backbone of the system test)
- `docs/TESTING.md` — a manual Jekyll preview guide, which the MkDocs migration supersedes

So TESTING.md cannot be a transcription of decode-orc's. That document describes a mature
gtest/ctest suite with a dozen labels and MVP boundary enforcement; writing the same shape
here would produce a document describing tests that do not exist. **TESTING.md must be honest
about what exists, what is planned, and what is inherently manual** — and it must be
per-component, because "unit test with mocked dependencies" means something very different
for a Qt application than for a Verilog module.

The good news: decode-orc's *dependency-inversion* philosophy is already partly present in
the GUI code. `ILogger.h`, `UsbDeviceBase` with `UsbDeviceLibUsb`/`UsbDeviceWinUsb`
subclasses, and header-only `StringUtilities` are all testable shapes today.

## 2. AGENTS.md — what transfers

decode-orc's structure maps onto DdD with three sections dropped, four adapted and three
added.

| decode-orc section | Action for DdD |
| --- | --- |
| 1 Overview & core constraints | **Adapt** — describe five components and their toolchains, not one app |
| 1.2 Git operations (no state-changing git without asking) | **Keep and harden** — promote to Rule 1 above, with the explicit command list |
| 1.3 Naming (no `phase0`-style names) | **Keep** |
| 1.4 Content restrictions (no attributions/ads) | **Keep and harden** — promote to Rule 2 above, with the explicit forbidden-string list |
| 2 MVP architecture | **Drop** — DdD has no MVP layering. Replaced by §2 below |
| 3 Security | **Keep**, trimmed |
| 4 Testing & QA | **Rewrite** — point at the per-component tiers in §3 below |
| 5 C++ coding standards | **Adapt** — DdD's existing style is long-form GPL headers, not SPDX (§5) |
| 6 Performance | **Adapt** — the relevant budget is sustained 40 Msps capture without dropped samples, not Big-O |
| 7 Dev environment & build | **Rewrite** for the single root flake (`nix develop .#gui`, from anywhere in the tree) |
| 8 Project structure | **Rewrite** for the post-reorganisation layout |
| 9 SDK & plugin rules | **Drop** — no plugins in DdD |
| 10 Documentation & specs | **Adapt** — point at `docs-tech/` and the docs site |
| 11 CI/CD | **Adapt** to the consolidated workflow |
| 12 Licensing | **Adapt** — and fix D17 first (§5) |
| 13 Contribution hygiene | **Keep** |
| 14 Implementation planning | **Keep** — it is the convention these very documents follow |
| 15–17 Guardrails, style, clarification triggers | **Keep** |
| 18 Common commands | **Rewrite** per component |

### New sections DdD needs that decode-orc does not

**A. Component boundaries.** DdD's equivalent of MVP enforcement is much simpler, and worth
stating because the monorepo makes violation easy for the first time:

- Each component (`gui/`, `fx3/`, `fpga/`, `hardware/`, `docs/`) builds independently.
- No cross-component source includes. Shared constants (USB VID/PID, control-bit
  assignments) are duplicated deliberately across the FPGA/firmware/host boundary because
  they are a *wire protocol*, not shared code — document the protocol in one place and note
  each definition site.
- Any change to the FPGA↔FX3↔host control protocol touches three components and must say so
  in the PR.

**B. Do not hand-edit generated or vendored files.** This one bites agents specifically:

| Path | Rule |
| --- | --- |
| `fx3/sdk/**` | Vendored Infineon SDK. Never reformat, never "fix" warnings |
| `fx3/firmware/src/domesday-duplicator-gpif.h` | Generated by GPIF II Designer. Regenerate, do not edit |
| `fpga/src/IPfifo.v`, `IPpllGenerator.v` | MegaWizard output, but **treated as source of truth** ([ide-independence.md](ide-independence.md) §2.2). Edit `defparam` values deliberately; do not reformat |
| `gui/src/DomesdayDuplicator/qcustomplot.{cpp,h}` | Vendored third-party library |

**C. Hardware safety.** Unique to this project and non-negotiable:

- Changes to gateware, FX3 firmware or the capture path can brick a device or, worse,
  silently corrupt captures. A green build is **not** sufficient evidence.
- Any such change requires the hardware-in-the-loop procedure in TESTING.md (§4) before
  merge.
- Never propose a change that writes to the FX3 EEPROM or FPGA EPCS as part of an automated
  test.

## 3. TESTING.md — per-component tiers

Rather than decode-orc's `unit`/`functional` split, DdD needs five tiers because the
components are so different. Every test is labelled with exactly one.

| Tier | Label | Meaning | Runs in CI |
| --- | --- | --- | --- |
| T1 | `unit` | Host-native, no I/O, no hardware, mocked dependencies, milliseconds | Yes |
| T2 | `golden` | Host-native, compares output against committed reference data | Yes |
| T3 | `sim` | Gateware simulation (Verilator/Icarus) | Yes |
| T4 | `static` | Lint, format, ERC/DRC, link checks, licence-header checks | Yes |
| T5 | `hil` | Hardware-in-the-loop; needs a real DdD, and usually a player | **No** — manual, gated on release |

### 3.1 `gui/` — the best return on effort

Host C++/Qt6, so ordinary unit testing applies. Highest-value targets first:

| Target | Tier | Why it is worth testing |
| --- | --- | --- |
| `dddconv` conversion | T1 + T2 | Pure data transformation between 10-bit packed and 16-bit sample formats, with a precise spec. Round-trip property tests plus golden files. A bug here silently corrupts every capture converted |
| `StringUtilities` | T1 | Header-only, pure, zero dependencies — the natural first test to prove the harness works |
| `amplitudemeasurement` | T1 | Pure computation over a sample buffer |
| `analysetestdata` logic | T1 | Feed synthetic buffers with deliberate sequence breaks; assert they are detected. This is the host half of the §4 oracle, so it must itself be trustworthy |
| `configuration` | T1 | Inject settings rather than touching `QSettings`' real backing store |
| Capture orchestration | T1 | `UsbDeviceBase` is already an abstraction — mock it and test the capture state machine without hardware |
| Dialogs | T1 (`gui-widget`) | QTest smoke coverage under `QT_QPA_PLATFORM=offscreen`, as decode-orc does |

Framework: GoogleTest (in nixpkgs, matches decode-orc, so contributors moving between the
two projects see one convention) driven by CTest.

### 3.2 `fx3/programmer/` — small but high-consequence

| Target | Tier | Why |
| --- | --- | --- |
| `find_flashprog_image()` | T1 | Directly testable path-resolution logic, and the site of **D13** |
| I2C/SPI paging arithmetic | T1 | Address stepping over `I2C_PAGE_SIZE` (64) and `SPI_FLASH_PAGE_SIZE` (256) boundaries is easy to get subtly wrong and hard to notice — a mis-paged write bricks the device |
| `.img` parsing and checksum | T1 + T2 | Golden file: a known-good `firmware.img` and its expected parse |
| Argument handling | T1 | Cheap |
| Actual USB transfers | T5 | Not worth mocking libusb at this scale |

### 3.3 `fx3/firmware/` — mostly T5, with one valuable exception

Bare-metal ARM, so most of it can only be tested on the device. But:

- **`generate-descriptor.sh` output is a golden-file test (T2).** Feed it a fixed commit
  string, compare the generated header byte-for-byte against a committed reference. This
  test would have caught **D8** (the double-stringified version producing
  `Domesday Duplicator ("abc12345")` with literal quotes) and will catch any future
  regression in the descriptor byte layout — including the length byte, which is computed in
  the script and consumed by the host.
- Pure logic that does not touch CyU3P APIs can be host-compiled and unit tested (T1). Today
  there is very little; keep it that way deliberately by pushing new logic into testable
  helpers.
- Everything touching the CyU3P SDK, DMA or GPIF: **T5**.

### 3.4 `fpga/` — simulation is free, so use it

`verilator` 5.048 and `iverilog` 13.0 are in nixpkgs, both free and cross-platform, so
gateware gets CI coverage even though bitstream builds cannot run there.

| Target | Tier | Notes |
| --- | --- | --- |
| All hand-written modules | T4 | `verilator --lint-only` — immediate, zero effort |
| `dataGenerator.v` | T3 | Assert the test-mode ramp is exactly 0…1020 then wraps (§4) |
| `fx3StateMachine.v` | T3 | The highest-risk module: the FX3 handshake. Worth a proper testbench |
| `statusLED.v` | T3 | Simple timing logic, easy win |
| `buffer.v` | T3 | Partial — see caveat |
| `DomesdayDuplicator.v` (top) | T3 partial / T5 | Full elaboration needs `dcfifo`/`altpll` simulation models from the vendor. Either stub them or restrict simulation to the surrounding logic; say which in TESTING.md rather than pretending whole-design simulation is free |

### 3.5 `hardware/` and `docs/`

- `hardware/`: `kicad-cli sch erc` and `kicad-cli pcb drc` as T4 — **blocked** on migrating
  the KiCad 5 files to the current format, since `kicad-cli` cannot read legacy `.sch`.
  Until then, ERC/DRC is manual and TESTING.md should say so plainly.
- `docs/`: `mkdocs build --strict` as T4, which subsumes the old link and orphan scripts.

### 3.6 Cross-cutting T4 checks

- `clang-format --dry-run --Werror` for C/C++
- `verible-verilog-format`/`lint` for Verilog
- Licence-header presence check. Only **8 of 67** source files carry SPDX identifiers today;
  the rest use the long-form GPL header. Decide one convention (§5) and enforce it

## 4. The system test that already exists

The most important thing in TESTING.md is a procedure the project already has all the parts
for but has never written down.

`dataGenerator.v` contains a **built-in test-pattern generator**. When `testModeFlag` is
asserted — `fx3_testMode = fx3_control[05]`, i.e. settable by the host over the FX3 control
interface — the FPGA substitutes a counter ramp for real ADC data:

```verilog
assign dataOut[9:0] = testModeFlag ? testData : adcData;
…
if (testData == 10'd1021 - 1) testData <= 10'd0;
else                          testData <= testData + 10'd1;
```

And `dddutil`'s `analysetestdata.cpp` walks a captured file checking that ramp is unbroken.

Together these form a **complete end-to-end integrity oracle**: enable test mode, capture,
analyse. Any discontinuity in the sequence proves a sample was dropped somewhere across
FPGA → FIFO → FX3 → USB 3.0 → host → disk. For a data-acquisition device, that single test
is worth more than any amount of unit coverage, because dropped samples are the failure mode
that matters and the one that is invisible in normal use.

TESTING.md must document it as the canonical T5 procedure:

1. `nix develop .#gui` and build, or use a released binary
2. Enable test mode in the capture application
3. Capture for a defined duration (specify one — long enough to exercise buffer wrap)
4. `dddutil` → analyse test data
5. **Pass = zero sequence breaks.** Any break is a release blocker

This procedure becomes the acceptance gate for P5-4 (firmware) and P6-5 (gateware), which
currently say only "a capture round-trips". It also gives simulation/hardware symmetry: the
T3 testbench for `dataGenerator.v` asserts the same 0…1020 ramp that the T5 procedure
verifies on real silicon.

Worth adding as follow-up work: a `--analyse-test-data` CLI mode so step 4 is scriptable
rather than GUI-driven, which would make the whole procedure semi-automated.

## 5. Licensing must be fixed before AGENTS.md is written (D17)

AGENTS.md needs a licensing section, and the current statements are wrong. Verified:

| | Actual | README says |
| --- | --- | --- |
| Software (`LICENSE`) | **GNU GPL v3** — confirmed from the file header, and matching the GPLv3 notices in the source files | "Software License - (Creative Commons BY-SA 4.0)", **linked to the GPLv3 file** |
| Hardware (`hardware/KiCAD/…/LICENSE.txt`) | **CC BY-SA 4.0** | "Hardware License - (GPLv3)", **linked to the CC BY-SA URL** |

The two licence *names* are transposed; each label points at the other one's file. Fix the
README before writing an AGENTS.md section that would otherwise codify the error.

Separately, decide the header convention: 8 of 67 source files use SPDX identifiers, the
rest long-form GPL notices. decode-orc mandates SPDX. Recommendation: **adopt SPDX** for
consistency across the two projects and because it is machine-checkable (§3.6), converting
files as they are touched rather than in one sweeping commit.

## 6. Tasks

| Task | Phase | Size | Detail |
| --- | --- | --- | --- |
| **P2-14** Fix the transposed licence statements | 2 | S | §5, D17 |
| **P2-15** Author `AGENTS.md` | 2 | M | §2, with the two non-negotiable rules (no automatic git operations, no AI attribution) stated **first**, before any other content. Lands with the re-layout so it describes the final structure |
| **P3-6** Test scaffolding + first host unit tests | 3 | M | `enable_testing()`, CTest labels T1–T5, GoogleTest from nixpkgs, wired into `nix flake check`. First tests: `StringUtilities`, `dddconv` round-trip, `find_flashprog_image` |
| **P3-7** Author `TESTING.md` | 3 | M | §3 and §4, marking clearly what exists versus what is planned |
| **P5-5** FX3 descriptor golden test | 5 | S | §3.3 — the test that catches D8 |
| **P6-7** Gateware testbenches | 6 | M | §3.4 — `dataGenerator`, `fx3StateMachine`, `statusLED` |
| **P7-6** CI test lanes | 7 | S | Run T1–T4 in the consolidated workflow; T5 never runs in CI |
| **P8-5** SPDX header convention rollout | 8 | M | §5 — convention plus the T4 check; convert opportunistically |

**Acceptance:** `nix flake check` runs every T1–T4 test for every component; TESTING.md
documents the T5 procedure precisely enough that someone with a DdD and a player can execute
it without asking questions; AGENTS.md describes the repository as it actually is after
Phase 2, with no reference to structures that no longer exist.
