# Gateware Style Guide and Enforcement (`fpga/`) — Implementation Plan

## Purpose

The gateware in [fpga/src/](../fpga/src/) is functionally proven — it has been in the field
since 2018, it is covered by three testbenches and a `verilator --lint-only -Wall` gate, and
its bitstream is byte-reproducible. What it does not have is an *agreed, mechanically
enforced style*. The result is a design where three naming conventions coexist inside one
module, indentation is tabs while the repository's own `.editorconfig` declares four spaces,
and comment columns line up only for a reader whose tab stop happens to match the author's.

This plan adopts a written style guide, moves the existing sources over to it in phases whose
risk is separately verifiable, and then makes it impossible to drift back — by adding a format
check and a style-lint check alongside the existing `fpga-lint` and `fpga-sim` checks, so that
style is enforced by the same `nix flake check` that already enforces correctness.

The scope is the **project-authored** Verilog only: five modules in [fpga/src/](../fpga/src/)
and three testbenches in [fpga/tests/](../fpga/tests/). The MegaWizard IP and its black-box
declarations are explicitly out of scope (§2.4).

## Evidence: the current state, measured

Everything below was measured against the working tree at `953e273`, using the `verible`
already present in `nix develop .#fpga`.

`verible-verilog-lint` at its default ruleset, over the five project modules and three
testbenches:

| Rule | Count | Nature |
| --- | --- | --- |
| `no-tabs` | 1203 | Tab indentation, against `.editorconfig`'s `indent_style = space` |
| `no-trailing-spaces` | 35 | Whitespace-only line endings |
| `explicit-parameter-storage-type` | 13 | `parameter x = ...` with no width or type |
| `explicit-task-lifetime` | 11 | Testbench tasks with no `automatic`/`static` |
| `module-filename` | 2 | Both are the `_bb.v` black boxes — out of scope (§2.4) |
| `forbid-defparam` | 2 | Both in `IPpllGenerator.v` — out of scope (§2.4) |
| `unpacked-dimensions-range-ordering` | 1 | `tb_spiRegisters.v` |
| `explicit-function-lifetime` | 1 | `readRegister` in `spiRegisters.v` |
| `case-missing-default` | 1 | `fx3StateMachine.v`, already waived in Verilator with a reason |
| `always-comb` | 1 | SystemVerilog-only advice — deliberately declined (§2.2) |

Naming, as declared across the five modules:

- `lowerCamelCase` — `nReset`, `dataIn`, `bufferOverflow`, `spiChipSelectN`, `shiftInNext`
- prefixed `snake_case` — `fx3_databus`, `fx3_control`, `adc_clock`, `sm_currentState`
- hybrids of the two — `pingUsedWords_wr`, `readData_flag`, `pingAsyncClear_reg`
- one near-miss pair — the `fx3StateMachine` output is `fx3isReading`, the top-level wire it
  connects to is `fx3_isReading`
- `ALL_CAPS` — `CLOCK_50`, `GPIO0`, `GPIO1`, `LED`, which are the DE0-Nano board's own pin
  names and are bound by name to the `.qsf` location assignments

`AGENTS.md` §5.3 currently states the convention as "`lowerCamelCase` signal names". That is
what roughly half the design does; this plan changes both the code and the statement.

### The migration was prototyped before this plan was written

A scratch copy of `fpga/src` and `fpga/tests` was formatted with
`verible-verilog-format --indentation_spaces=4 --column_limit=100`, and the project's own
gates were then run against the result:

```
run-lint.sh  → All 5 modules lint clean       (verilator --lint-only -Wall)
run-sim.sh   → All 3 testbenches passed       (iverilog)
```

So the mechanical part of this plan is known to work rather than hoped to. Two findings from
that trial shape the phasing below:

1. The formatter fixes 1101 of the 1203 `no-tabs` findings. The remaining **102 are tabs
   inside comments** — the licence header blocks and the FX3 signal-map table in
   `DomesdayDuplicator.v` — which no formatter will touch, because a formatter must not
   rewrite comment interiors. Those need a deliberate manual pass (Phase 2).
2. `version.vh` is unchanged by the formatter, so `generate-version.sh` and the
   `fpga-version` check need no adjustment for formatting.

## Authoritative references (in-tree)

- Repository conventions, the rule this plan rewrites: [AGENTS.md](../AGENTS.md) §3, §5.3, §5.4
- Licence-header gate and its exemption list: [tools/check-licence-headers.sh](../tools/check-licence-headers.sh)
- Hardware-in-the-loop gate for gateware changes: [TESTING.md](../TESTING.md) §5; [AGENTS.md](../AGENTS.md) §4
- Bitstream reproducibility, and what a byte-identical `.jic` does and does not prove:
  [fpga/README.md](../fpga/README.md), "Reproducibility"
- Existing gates this plan extends: [fpga/checks.nix](../fpga/checks.nix),
  [fpga/tests/run-lint.sh](../fpga/tests/run-lint.sh), [fpga/verilator-waivers.vlt](../fpga/verilator-waivers.vlt)
- Dev shell that already ships the tooling: [fpga/shell.nix](../fpga/shell.nix)
- The C++ precedent this plan deliberately mirrors: `.clang-format` as a `--dry-run --Werror`
  build gate, described in [docs-tech/ddd-gui-implementation-plan.md](ddd-gui-implementation-plan.md)

## 1. The choice of style guide

**Adopt the [lowRISC Verilog Coding Style Guide](https://github.com/lowRISC/style-guides/blob/master/VerilogCodingStyle.md)
as the written standard, and Verible as its mechanical enforcer.**

The reasoning, briefly, because "recommended best practice" deserves an argument rather than
an assertion:

- There is no ratified Verilog style standard the way there is for C++. What exists in
  practice is a small number of published house styles, of which lowRISC's is the one with the
  widest adoption outside its own organisation — it is the style of OpenTitan and Ibex, and it
  is the style **Verible's default rule set already encodes**. Choosing anything else means
  fighting the tool.
- Verible is the Verilog analogue of `clang-format`: one binary that both formats
  (`verible-verilog-format`) and style-lints (`verible-verilog-lint`), from a config file
  checked into the repository. This repository already chose that shape for C++ — Google style
  via `.clang-format`, enforced as a build gate — so the Verilog gate is the same idea in the
  same place, not a new mechanism to learn.
- It is already installed. `fpga/shell.nix` puts `verible` in `nix develop .#fpga` and the
  shellHook already prints `verible-verilog-format --inplace <file>` as a suggestion. This plan
  finishes a job the tooling was set up for and then never had a policy behind it.

lowRISC is adopted **as a base, with four recorded deviations**. Each is written into the
style document with its reason, because an unexplained deviation is indistinguishable from
someone not having read the guide.

### 1.1 Recorded deviations from lowRISC

| lowRISC says | This project does | Why |
| --- | --- | --- |
| Two-space indent | **Four-space indent** | [.editorconfig](../.editorconfig) already declares `indent_size = 4` for `*.v`, matching the C, C++, Python and shell settings across the repository. A tree-wide indent convention is worth more than agreement with an external document on a point that carries no meaning. |
| SystemVerilog: `logic`, `always_ff`, `always_comb` | **Verilog-2001: `reg`/`wire`, `always @(posedge …)`** | Every entry in `DomesdayDuplicator.qsf` is `VERILOG_FILE`, and Quartus Prime Lite's SystemVerilog support is partial. Converting a design that has been capturing correctly since 2018 to gain compile-time latch detection is a poor trade against the risk. The Verible rules that assume SystemVerilog (`always-comb`, `explicit-task-lifetime`, `explicit-function-lifetime`) are disabled in the config, each with this reason attached. |
| Module and file names `lower_snake_case` | **Existing module and file names unchanged** | `DomesdayDuplicator` is `TOP_LEVEL_ENTITY` in the `.qsf` and the stem of `.sof`/`.jic`/`.cdf`/`.cof` filenames, of the programming instructions, and of `bitstream-provenance.py`'s expectations. `dataGenerator`, `buffer`, `fx3StateMachine` and `spiRegisters` are named in the `.qsf`, in `run-lint.sh`, in `run-sim.sh` and in the licence-header exemption list. The blast radius is large and the gain is cosmetic. See §5 — this is the one open item. |
| Port suffixes `_i`/`_o`, flop suffixes `_d`/`_q` | **Not adopted** | Verible's `port-name-suffix` and `dff-name-style` rules are off by default and stay off. These conventions pay for themselves in a large multi-team design with deep hierarchy; this is five modules, one clock domain crossing, and a port list a reader can hold in their head. |

### 1.2 The naming rules, stated

| Kind | Convention | Example |
| --- | --- | --- |
| Nets, variables, ports | `lower_snake_case` | `spi_chip_select_n`, `buffer_overflow` |
| Active-low signals | trailing `_n` | `reset_n`, `spi_chip_select_n` |
| `parameter` / `localparam` | `UpperCamelCase` | `BufferSize`, `CommitText`, `StateSendPacket` |
| Testbench constants | `ALL_CAPS` permitted | `RAMP_LENGTH`, `HALF_BIT` |
| Macros / `` `define `` | `ALL_CAPS` | `GATEWARE_COMMIT_TEXT` — already compliant |
| Module instances | `lower_snake_case` | `spi_registers_0`, `ping_buffer` |
| Module and file names | unchanged (§1.1) | `spiRegisters`, `fx3StateMachine` |
| Top-level ports | unchanged | `CLOCK_50`, `GPIO0`, `GPIO1`, `LED` |

One structural rule belongs with these, because the formatter's behaviour depends on it:
**every `if`/`else` body gets `begin`/`end`, however short it is.** lowRISC requires it, and
without it the formatter collapses a short body onto its condition's line whenever it fits
inside the column limit — so whether two adjacent, structurally identical conditionals look the
same comes down to how long their signal names are. Enforced as `explicit-begin`, applied in
Phase 4 (see "What Phase 1 surfaced").

The top-level ports are the one place where the rule is suspended by necessity rather than by
preference. `DomesdayDuplicator.qsf` carries 164 lines that bind these four names to physical
pins and I/O standards; renaming them means editing every one of those lines, and a single
typo produces a board that programs successfully and drives the wrong pin. They stay, and the
style document says so in one sentence so that nobody "fixes" them later.

The `parameter`/`localparam` rule needs no configuration: Verible's default
`parameter-name-style` accepts `UpperCamelCase` and `ALL_CAPS` and rejects everything else —
verified empirically to flag `bufferSize`, `commitText`, `buildFlags`, `state_waitForRequest`
and `state_sendPacket` while accepting the testbenches' existing `RAMP_LENGTH` and `HALF_BIT`.

### 1.3 Licence headers

The header block is part of the file's style, so the style guide states its exact shape rather
than leaving it to imitation. This is not a new convention: [AGENTS.md](../AGENTS.md) §5.4
already defines it repository-wide, and the gateware headers have simply drifted from it in two
respects — they are tab-indented where the template uses four spaces, and they place the blank
line after the description rather than after the filename.

**The canonical header for a `.v` / `.vh` file**, which is §5.4's template with the gateware's
existing two-line title convention:

```verilog
/************************************************************************

    spiRegisters.v

    SPI register bank
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Any longer explanation of what the module does and why it is shaped
    this way goes here, after the SPDX pair.

************************************************************************/
```

For `.sh`, `.nix`, `.py` and `.vlt` files in `fpga/`, §5.4's `#`-comment form applies unchanged
— shebang first where there is one, then the description paragraph, then the project and SPDX
lines.

Rules, all of them already §5.4's and repeated here only because Phase 2 acts on them:

- Four-space indent inside the block, never tabs. This is what makes the header agree with
  both the formatter and [.editorconfig](../.editorconfig).
- The SPDX pair is `SPDX-FileCopyrightText` then `SPDX-License-Identifier: GPL-3.0-or-later`,
  in that order, adjacent, immediately after the project line.
- Do not add a header to `IPfifo.v`, `IPfifo_bb.v`, `IPpllGenerator.v` or
  `IPpllGenerator_bb.v`. They are exempt by name in
  [tools/check-licence-headers.sh](../tools/check-licence-headers.sh) because a MegaWizard
  regeneration would overwrite it.

#### The copyright year

**The thirteen files in `fpga/` that read `2018-2025` become `2018-2026`.** These are
`DomesdayDuplicator.v`, `buffer.v`, `dataGenerator.v`, `fx3StateMachine.v`,
`DomesdayDuplicator.SDC`, `tb_dataGenerator.v`, `tb_fx3StateMachine.v`, `run-lint.sh`,
`run-sim.sh`, `test_provenance.py`, `bitstream-provenance.py`, `build-local.sh` and
`verilator-waivers.vlt`.

**The nine files created in 2026 keep `2026`** — `spiRegisters.v`, `version.vh`,
`tb_spiRegisters.v`, `shell.nix`, `package.nix`, `checks.nix`, `quartus-shell.nix`,
`generate-version.sh` and `run-version.sh`. §5.4 requires "the years that author actually
worked on the file", and `spiRegisters.v` did not exist in 2018. A uniform `2018-2026` across
the directory would read more tidily and would be wrong.

The three files outside `fpga/` that also still read `2018-2025` —
`fx3/firmware/src/domesday-duplicator.c`, its header, and `usb-descriptor.c` — are **out of
scope**. §5.4's existing rule covers them: refresh the header of a file you are already
editing, not in a sweep of its own. Putting a firmware diff inside a gateware-style change
would also drag in the firmware's own hardware-verification expectation for no benefit.

#### The year is not machine-checked, deliberately

`tools/check-licence-headers.sh` greps for the presence of a copyright statement and a licence
statement; it does not look at the year. That is why `2018-2025` and `2026` have coexisted
inside one directory without any gate noticing.

**This stays as it is.** A check that asserts the end year is the current year turns every 1
January into a tree-wide sweep and fails CI on files nobody has touched, which trains people to
edit headers to appease a tool rather than to record a fact. The year is a convention
maintained at review time; this plan refreshes it because it is refreshing these files anyway.

## 2. The tooling

### 2.1 `fpga/.verible-format` — formatter settings

Committed as an abseil flag file — `verible-verilog-format --flagfile=…` — so that the dev
shell, the check script and any editor integration cannot disagree about what "formatted"
means:

```
--indentation_spaces=4
--column_limit=100

--assignment_statement_alignment=align
--case_items_alignment=align
--class_member_variable_alignment=align
--distribution_items_alignment=align
--enum_assignment_statement_alignment=align
--formal_parameters_alignment=align
--module_net_variable_alignment=align
--named_parameter_alignment=align
--named_port_alignment=align
--port_declarations_alignment=align
--struct_union_members_alignment=align

--verify_convergence=true
```

Alignment is **on**, per the decision recorded for this plan. The formatter owns column
alignment outright, which is what stops anyone hand-tabbing a comment into a column again. The
cost is accepted and worth stating: adding one long signal name re-aligns its neighbouring
group, so occasional diffs touch lines the author did not edit.

**Every** alignment mode is listed, including the ones this design has no occurrences of
(`class_member_variable`, `distribution_items`, `enum_assignment_statement`,
`struct_union_members`). Verible's default for each is `infer`, which guesses the intent from
how the file is already laid out — making the output depend on its own input, which is the
non-determinism this file exists to remove. Leaving a mode at `infer` because "there are none
of those today" would mean the first one someone writes is formatted by a guess.

`--column_limit=100` rather than the `.editorconfig`'s `max_line_length = 120`: the sources
already fit inside 100 columns (measured — zero lines exceed it today), and a tighter limit
that costs nothing is worth keeping.

`--verify_convergence=true` is Verible's default, written down because a non-convergent
formatter — one whose output differs from the format of its own output — would make the CI
check flap between passing and failing on an unchanged file.

### 2.2 `fpga/.rules.verible_lint` — style-lint rule set

Verible's default rules, minus the SystemVerilog-only ones, plus the naming rule that is off
by default and is the whole point of the naming migration:

```
# Enforce the naming convention of the style guide (off by default in Verible).
+signal-name-style

# SystemVerilog-only advice. This design is Verilog-2001 because the .qsf declares
# every file as VERILOG_FILE and Quartus Prime Lite's SystemVerilog support is
# partial — see the gateware style guide, "Recorded deviations".
-always-comb
-explicit-task-lifetime
-explicit-function-lifetime

# Covered, with a written justification, by fpga/verilator-waivers.vlt: the case is
# preceded by `sm_nextState = sm_currentState`, so every uncovered state holds.
# Duplicating the waiver here would mean two places to keep in step.
-case-missing-default
```

Every disabled rule carries its reason inline, matching the rule
[verilator-waivers.vlt](../fpga/verilator-waivers.vlt) already sets: *a waiver with no reason
is indistinguishable from a bug someone hid*. The file is scoped to the project-authored
sources; the IP files are never passed to it (§2.4).

Verible finds a file of this exact name by searching upward from each analysed source, so
`verible-verilog-ls` — the language server already in the `.#fpga` dev shell — picks it up with
no editor configuration at all. That is the reason for the leading dot and the fixed name.

### 2.3 `fpga/verible-waivers` — narrow per-case exceptions

A third file, for findings that are correct about the code but wrong about what the code
*should* be. The distinction against `.rules.verible_lint` is worth keeping sharp: a rule that
is wrong **everywhere** is disabled once, there, with its reason; a rule that is right in
general but wrong in one named place is waived here.

It has one entry, and on present evidence should never need many:

```
waive --rule=signal-name-style --regex="\b(CLOCK_50|GPIO0|GPIO1|LED)\b" --location=".*DomesdayDuplicator\.v"
```

`CLOCK_50`, `GPIO0`, `GPIO1` and `LED` are the only signals in the design not free to be
renamed (§1.2). Note the scoping: `--regex` matches the text of the offending line, so the
waiver names the four identifiers being excused rather than switching the rule off for the
file. A new ALL_CAPS signal — including a new port on this same module — is still reported.
A bare `--location` would have been one word shorter and would have hidden it.

### 2.4 What is deliberately not covered

`IPfifo.v`, `IPfifo_bb.v`, `IPpllGenerator.v` and `IPpllGenerator_bb.v` are excluded from both
the formatter and the style linter. [AGENTS.md](../AGENTS.md) §3 already says of the first
pair: *"Originally MegaWizard output, but treated as source of truth. Change `defparam` values
deliberately; do not reformat."* This plan does not weaken that, and the check scripts list the
five project modules by name rather than globbing `src/*.v`, so a future MegaWizard regeneration
cannot silently pull vendor output into the gate. The four `module-filename` and
`forbid-defparam` findings in the table in *Evidence* are all in these files and are correctly
never seen.

`DomesdayDuplicator.SDC` is left alone as TCL — it is three constraint lines and a comment
header, and Verible does not read TCL. Its tab-indented comment header is handled by the
Phase 2 comment pass along with the Verilog headers.

## 3. The migration, in phases

The phases are ordered so that **each has a different and separately checkable risk profile**.
That matters more than convenience here, because [AGENTS.md](../AGENTS.md) §4 is explicit that
a green build is not sufficient evidence for a gateware change. Splitting whitespace from
renaming means the whitespace phase can be proven inert by a much stronger argument than the
renaming phase can, and the two do not have to share the weaker one.

### Phase 0 — Land the tooling, not yet the gate

1. Add `fpga/.verible-format`, `fpga/.rules.verible_lint` and `fpga/verible-waivers`
   (§2.1, §2.2, §2.3).
2. Add `fpga/tests/run-style.sh`, modelled line-for-line on the existing
   [run-lint.sh](../fpga/tests/run-lint.sh): same header, same `set -euo pipefail`, same
   named-not-globbed file list, same one-line-per-file output, same "here is what to do about a
   finding" failure message. It runs two things over the five modules and three testbenches:
   - `verible-verilog-format --verify` (formatting is exactly what the config produces)
   - `verible-verilog-lint --rules_config=… --waiver_files=…` (style rules)
3. Add `fpga/tests/run-format.sh` — the same file list, `--inplace`. This is the fixer, and it
   is what a contributor runs when the check fails. It is deliberately *not* a Nix check: a
   check that edits the tree is not a check.
4. Extend `fpga/shell.nix`'s shellHook to print `./fpga/tests/run-style.sh` and
   `./fpga/tests/run-format.sh` alongside the existing `run-lint.sh` and `run-sim.sh` lines,
   replacing the two loose `verible-verilog-*` suggestions that have no config behind them,
   and to name the three config files so their existence is discoverable from the shell.

The gate is **not** wired into `checks.nix` yet — at this point it would fail, loudly, on every
file. Phase 0 is landable on its own and leaves the tree green.

*Verification:* `./fpga/tests/run-style.sh` runs and reports the failures from *Evidence*. That
it fails is the correct outcome.

### Phase 1 — Mechanical reformat (whitespace only, no identifier changes)

Run `./fpga/tests/run-format.sh` over the five modules and three testbenches. Nothing else in
this phase — no renames, no comment edits, no logic changes.

This is the largest diff in the plan and the least risky. Every hunk is the formatter's output
from a checked-in config, reproducible by anyone with the same Verible.

*Verification:*
- `run-lint.sh` and `run-sim.sh` both pass. **Already demonstrated** on the prototype (see
  *Evidence*).
- `run-style.sh`'s format half passes; its lint half still reports the comment-interior tabs.
- **The cheap proof that it is whitespace-only:** strip every whitespace character from each
  file and compare its hash against the same file at `HEAD`. If the two agree, not one token
  was added, removed or reordered — which is a stronger statement than
  `git diff --ignore-all-space` being empty, and unlike that diff it stays true when the
  formatter re-wraps a line.

  ```bash
  for f in fpga/src/*.v fpga/tests/tb_*.v; do
      a=$(git show "HEAD:$f" | tr -d '[:space:]' | sha256sum)
      b=$(tr -d '[:space:]' < "$f" | sha256sum)
      [ "$a" = "$b" ] || echo "TOKENS CHANGED: $f"
  done
  ```

- **The strong check:** build a `.jic` before and after and compare. [fpga/README.md](../fpga/README.md)
  establishes that `DomesdayDuplicator.jic` is byte-identical across rebuilds of the same
  commit with the same Quartus, so a whitespace-only change that produces a byte-identical
  `.jic` has been *proven* inert, not argued to be. `./bitstream-provenance.py --build-dir …`
  produces the canonical digest to compare.

If the `.jic` does not match, stop: something in this phase was not whitespace.

#### What Phase 1 surfaced: collapsed `if`/`else` bodies

The formatter puts a short unbraced conditional body on the same line as its condition when it
fits inside the column limit. Six sites are affected, four in `dataGenerator.v` and two in
`spiRegisters.v`:

```verilog
// before                              // after
if (testData == 10'd1021 - 1)          if (testData == 10'd1021 - 1) testData <= 10'd0;
    testData <= 10'd0;                 else testData <= testData + 10'd1;
else
    testData <= testData + 10'd1;
```

This is token-identical and the hash check above confirms it, but it is a readability
regression, and worse it is an *inconsistent* one: in `spiRegisters.v` the collapsed
`if (spiClockSync[1] == spiClockSample)` now sits directly above an
`if (spiChipSelectNSync[1] == spiChipSelectNSample)` that stayed on two lines purely because
its longer name pushes it past 100 columns. Identical constructs, formatted differently by an
accident of name length.

No formatter flag suppresses this — the fix is the code, not the config, and it is the rule
lowRISC states anyway: **conditional bodies always get `begin`/`end`**. Verible enforces it as
`explicit-begin`, which is off by default. Since inserting `begin`/`end` is a source edit
rather than a whitespace one it does not belong in this phase; it is added to Phase 4, and
§1.2 gains the rule. Until then the six sites stay collapsed, which is ugly and harmless.

### Phase 2 — Comment blocks and licence headers

The 102 residual tabs and the remaining trailing whitespace are all inside comments, and **64
of the 102 are the licence header blocks** — eight tabbed lines in each of the eight project
files, identically. So the header normalisation of §1.3 and the comment detabbing are the same
edit, and belong in the same phase.

1. **The licence header block** at the top of every project-authored file in `fpga/` — the
   eight `.v`, `version.vh`, the `.SDC`, and the `.sh`, `.nix`, `.py` and `.vlt` tooling.
   Bring each to the canonical shape of §1.3: four-space indent instead of tabs, blank line
   after the filename, the SPDX pair adjacent and in order. Wording is preserved; only layout
   changes.
2. **The copyright year**, per §1.3: `2018-2025` → `2018-2026` on the thirteen files listed
   there. The nine 2026 files are left alone, as are the four MegaWizard IP files, which have
   no header by design and are exempt by name in `tools/check-licence-headers.sh`.
3. `generate-version.sh`'s heredoc, which *writes* a tab-indented header into the generated
   `version.vh`. Fixing the checked-in `version.vh` without fixing the generator means the next
   build silently reintroduces the tabs — and the generated header must come out matching §1.3
   exactly, because the `fpga-version` check compiles what it produces.
4. The FX3 signal-map table in `DomesdayDuplicator.v` (lines 135–153) — a five-column table
   held together by tabs, which only aligns at one tab width. Retab to spaces at the column
   positions it was clearly intended to have.
5. `run-version.sh`'s embedded `parses.v` heredoc, which is tab-indented Verilog.
6. Fix the trailing space on `// High-Z the unused FX3 databus pins ` and its few siblings.

*Verification:*
- `run-style.sh` reports zero `no-tabs` and zero `no-trailing-spaces` findings across the eight
  project files.
- `tools/check-licence-headers.sh` passes, and its printed count of remaining long-form GPL
  notices is unchanged — this phase converts no notice, it only re-lays-out SPDX ones that
  already exist.
- `run-lint.sh` and `run-sim.sh` pass. The `fpga-version` check passes, which is the one that
  matters here because `generate-version.sh` changed: it regenerates `version.vh` for six
  commit cases and compiles the result.
- `git diff --ignore-all-space` over the `.v` files shows only the year change and the SPDX
  line reordering — that is the machine-checkable statement that nothing but layout moved.
- The `.jic` is expected to remain byte-identical; comments do not reach the synthesiser.

#### What Phase 2 surfaced

Four things the detabbing exposed, none of them anticipated when this phase was written. All
are comment-only and none reaches the synthesiser.

1. **Two headers named the wrong file.** `DomesdayDuplicator.v` announced itself as
   `Domesday Duplicator.v` — with a space, which is not the filename and never was — and
   `dataGenerator.v` announced itself as `dataGeneration.v`, a module name that does not
   exist. Both corrected. This is the sort of thing that survives indefinitely in a header
   nobody has cause to read closely, and it is a small argument for the `module-filename`
   rule that §1.1 declines for a different reason.
2. **`version.vh` was regenerated rather than hand-edited.** Running the fixed
   `generate-version.sh` over `fpga/src` is what produced the committed copy, so the generator
   and the file it generates agree *by construction* rather than by two edits that happen to
   match. The `fpga-version` check then compiles the result.
3. **The signal-map table gained a header row** — `Signal / GPIO / CTL / Direction /
   Description`. Retabbing the columns without naming them would have preserved a table whose
   meaning had to be inferred from its contents. The blank lines between the three groups
   became `//`, so the table is one contiguous comment block rather than four.
4. **`DomesdayDuplicator.SDC` had a C-comment artefact.** Its box terminated with
   `#***…***/` — a stray `/` carried over from the `.v` headers this was copied from, in a
   file where `#` is the comment character and `*/` means nothing. Removed. Its description
   line also read "SDC file", which restates the extension rather than saying what the file
   does; it now reads "Timing constraints".

### Phase 3 — Naming migration to `lower_snake_case`

The substantive phase. Every identifier below is renamed **atomically across all eight files
plus the two documentation pages that name them**, because a half-applied rename does not
compile and a rename applied to code but not to prose leaves documentation that lies.

**`DomesdayDuplicator.v`** — `fx3_nReset` → `fx3_reset_n`, `fx3_dataAvailable` →
`fx3_data_available`, `fx3_readData` → `fx3_read_data`, `fx3_bufferError` →
`fx3_buffer_error`, `fx3_spiClock` → `fx3_spi_clock`, `fx3_spiMosi` → `fx3_spi_mosi`,
`fx3_spiMiso` → `fx3_spi_miso`, `fx3_spiChipSelectN` → `fx3_spi_chip_select_n`,
`fx3_testMode` → `fx3_test_mode`, `fx3_isReading` → `fx3_is_reading`, `dataGeneratorOut` →
`data_generator_out`. Instances → `pll_generator_0`, `data_generator_0`, `buffer_0`,
`fx3_state_machine_0`, `spi_registers_0`. Already compliant and untouched: `fx3_databus`,
`fx3_control`, `fx3_clock`, `adc_databus`, `adc_clock`. Untouched by rule: `CLOCK_50`,
`GPIO0`, `GPIO1`, `LED`.

**`buffer.v`** — ports `nReset`/`writeClock`/`readClock`/`isReading`/`dataIn`/
`bufferOverflow`/`dataAvailable`/`dataOut` → `reset_n`/`write_clock`/`read_clock`/
`is_reading`/`data_in`/`buffer_overflow`/`data_available`/`data_out`. Internals
`currentWriteBuffer`, `bufferOverflowHold`, the twelve `ping*`/`pong*` pairs
(`pingUsedWords_wr` → `ping_used_words_wr`, `pingdataOut` → `ping_data_out`, and so on) and
`pingAsyncClear_reg`/`pongAsyncClear_reg` → their snake equivalents. `localparam bufferSize`
→ `BufferSize`. Instances `pingBuffer`/`pongBuffer` → `ping_buffer`/`pong_buffer`.

**`dataGenerator.v`** — `nReset` → `reset_n`, `testModeFlag` → `test_mode_flag`, `dataOut` →
`data_out`, `adcData` → `adc_data`, `testData` → `test_data`, `sequenceCount` →
`sequence_count`.

**`fx3StateMachine.v`** — `nReset` → `reset_n`, `readData` → `read_data`, `readData_flag` →
`read_data_flag`, `fx3isReading` → `fx3_is_reading` (which also closes the
`fx3isReading`/`fx3_isReading` mismatch noted in *Evidence*), `sm_currentState` →
`sm_current_state`, `sm_nextState` → `sm_next_state`, `wordCounter` → `word_counter`,
`state_waitForRequest` → `StateWaitForRequest`, `state_sendPacket` → `StateSendPacket`.

**`spiRegisters.v`** — ports `nReset`/`spiClock`/`spiMosi`/`spiChipSelectN`/`spiMiso`/
`testMode` → `reset_n`/`spi_clock`/`spi_mosi`/`spi_chip_select_n`/`spi_miso`/`test_mode`
(`clock` and `leds` already comply). Parameters `commitText` → `CommitText`, `buildFlags` →
`BuildFlags`. Function `readRegister` → `read_register`, its input `readAddress` →
`read_address`. Internals: the six `spi*Sync`/`spi*Sample`/`spi*Level` signals,
`spiClockLevelPrevious`, `spiClockRising`, `spiClockFalling`, `testModeRegister`,
`ledRegister`, `shiftIn`, `shiftOut`, `shiftInNext`, `bitCount`, `commandReceived`,
`readTransfer`, `addressNext`, `misoOut` → their snake equivalents.

**The three testbenches** — internal signals and every `.port(…)` connection, in lockstep.
`tb_spiRegisters.v` additionally updates `.commitText(COMMIT_TEXT)` → `.CommitText(COMMIT_TEXT)`
and `.buildFlags(BUILD_FLAGS)` → `.BuildFlags(BUILD_FLAGS)`. Its existing `ALL_CAPS` constants
stay as they are (§1.2).

**Not renamed, and confirmed by inspection to need no change:** the `` `define `` macros
`GATEWARE_COMMIT_TEXT` and `GATEWARE_BUILD_FLAGS` are already `ALL_CAPS`, so
`generate-version.sh`, `run-version.sh` and `version.vh` are untouched by this phase.

**Documentation that names these identifiers, updated in the same commit** — but see "What
Phase 3 surfaced" below, which corrects this list:
[docs/content/development/software-guide.md](../docs/content/development/software-guide.md)
lines 134–144 (the FX3 signal list), [docs/content/development/fpga-register-interface.md](../docs/content/development/fpga-register-interface.md)
lines 13 and 117, [TESTING.md](../TESTING.md) lines 384–389 (which quotes the
`assign dataOut[9:0] = testModeFlag ? …` line verbatim), and [AGENTS.md](../AGENTS.md) §5.3.

*Verification:*
- `run-lint.sh`, `run-sim.sh`, `run-style.sh` all pass. `signal-name-style` reporting zero
  findings is the machine-checkable statement that the rename is complete.
- `grep -rn` for each old identifier across the whole tree returns nothing outside
  `docs/site/` (generated) and git history.
- **Hardware-in-the-loop (TESTING.md §5) is required for this phase.** Unlike Phase 1, a
  byte-identical `.jic` is *not* a safe assumption here — node names feed the fitter, and
  whether they perturb it is an empirical question this project's own culture says to measure
  rather than assert (fpga/README.md, "Measured, not assumed"). Build the `.jic`; if it is
  byte-identical, record that as a pleasant surprise and still run the capture-integrity test.
  If it differs, the test-pattern analysis over a real capture with zero sequence breaks is the
  only evidence that counts.

#### What Phase 3 surfaced

**1. Most of the documentation this phase was told to update must not be touched.** The plan
listed `software-guide.md` lines 134–144 and `fpga-register-interface.md` lines 13 and 117 as
naming the renamed identifiers. They do — but they are not naming the *Verilog*. They are
naming the **GPIF signals**, and `dataAvailable`, `collectData` and `readData` appear verbatim
in `fx3/firmware/gpif/DomesdayDuplicator.cydsn/projectfiles/gpif2model.xml`, the GPIF II
Designer project that [AGENTS.md](../AGENTS.md) §3 marks generated and not to be edited. The
`software-guide.md` list sits directly under a screenshot of the Cypress I/O Matrix view whose
labels those are.

This is precisely the case [AGENTS.md](../AGENTS.md) §2 describes: *"the control-bit
assignments appear separately in the gateware, the firmware and the host software… it is a wire
protocol, and the three definitions live in three different languages"*. Renaming the FPGA-side
identifiers does not rename the GPIF-side ones, and editing the prose to match the Verilog would
make it disagree with the tool a reader has open. **Both files are left alone.** Only genuine
Verilog references were updated:

- `TESTING.md` §5 — a verbatim `assign dataOut[9:0] = testModeFlag ? testData : adcData;` quote,
  which is gateware source and had to move.
- `AGENTS.md` §5.3 — rewritten to the convention table of §1.2 (this was always Phase 5's item;
  doing it here keeps the statement true at every commit rather than for two phases being false).

**2. `TESTING.md` carried a stale factual claim, now corrected.** It said `fx3_testMode` *is*
`fx3_control[05]`. That stopped being true when the SPI register bank landed: `fx3_control[05]`
is now `fx3_spi_clock`, and test mode arrives over SPI at register `0x10`. The sentence was
being edited for the rename anyway, and renaming the identifiers inside a false sentence would
have left it false and newer-looking.

**3. Two comments in `fx3StateMachine.v` named things that do not exist.** "Set state to
`state_idle` on reset" — there has never been a `state_idle`; the reset state is
`StateWaitForRequest`. And "Counter for the `sendPacket` state", where the parameter is
`StateSendPacket`. Both corrected.

**4. The signal-map table had to be rebuilt, not just re-spaced.** `spiChipSelectN` (14) became
`spi_chip_select_n` (17) and overflowed the column Phase 2 had set. Rebuilt at an 18-column
name field; the longest row is now 99 characters. Worth noting that Verible's `line-length`
rule is enabled by default at 100, so the table has one character of headroom — a further
lengthening of a signal name in that table will need the description column shortened.

**5. The predicted wart is real and looks as expected.** `dataGenerator data_generator_0 (` is
what §5's open item describes. It reads oddly but it is honest: the module name is pinned by
the `.qsf` and the instance name is not.

### Phase 4 — Residual style findings

The findings that are neither whitespace nor naming, each a small deliberate edit:

- `explicit-parameter-storage-type` (13). Give every `parameter`/`localparam` an explicit width:
  `localparam [13:0] BufferSize = 14'd8191;`, `parameter [63:0] CommitText = …;`,
  `parameter [7:0] BuildFlags = …;`, and the testbench constants. This is the one item in the
  plan with any semantic content at all — an untyped `parameter` takes its width from its
  initialiser, so writing the width down is making an existing fact explicit, and the
  testbenches are what confirm it.
- `state_waitForRequest`/`state_sendPacket` become `localparam [3:0]` rather than `parameter`
  while being renamed in Phase 3 — they are state encodings, not knobs an instantiation should
  override.
- `unpacked-dimensions-range-ordering` (1) in `tb_spiRegisters.v`: `[0:N-1]` → `[N]`.
- **`explicit-begin` (6)** — the six collapsed conditionals Phase 1 produced (four in
  `dataGenerator.v`, two in `spiRegisters.v`). Give every `if`/`else` body a `begin`/`end`,
  which is lowRISC's rule, restores the four-line form the sources had, and makes the layout
  stop depending on how long the signal names happen to be. Enable `+explicit-begin` in
  `.rules.verible_lint` in the same change, so the fix cannot silently regress. Re-run
  `run-format.sh` afterwards: with `begin` present the formatter keeps the bodies on their own
  lines, so formatting and style agree rather than fighting.

`TESTING.md` §5 quotes the `dataGenerator.v` ramp verbatim, including the two collapsed
conditionals. Re-check that quote after this phase — adding `begin`/`end` changes the lines it
reproduces, and a code quote that no longer matches the code is worse than no quote.

#### What Phase 4 surfaced

**Two of the three rules this phase set out to satisfy cannot be satisfied in Verilog-2001, and
were disabled instead.** This inverts what the phase was written to do, so the evidence is
recorded rather than summarised.

`explicit-parameter-storage-type` wants a storage *type*, and an explicit width is not one —
`localparam [13:0] BufferSize` is still flagged. Every form the rule accepts was tested against
`iverilog -g2005`, the standard `run-sim.sh` compiles with:

| Form | Verible | `iverilog -g2005` | Why not |
| --- | --- | --- | --- |
| `logic [13:0]` | accepts | accepts | SystemVerilog — the §1.1 deviation |
| `bit [13:0]` | accepts | **rejects** | SystemVerilog |
| `reg [13:0]` | accepts | accepts | not in the IEEE 1364-2001 parameter grammar; a bet on what Quartus tolerates |
| `integer` | accepts | accepts | loses the width, and is 32-bit — wrong for the 64-bit `CommitText` |
| `signed [13:0]` | accepts | accepts | makes unsigned constants signed, changing the comparisons they appear in |

`unpacked-dimensions-range-ordering` wants `reg [7:0] read_data [16]` for `[0:15]`. The
bare-size form is SystemVerilog; `iverilog` accepts it only with a *"Use of SystemVerilog
[size] dimension"* warning, and `run-sim.sh` runs `-Wall`.

Both are therefore the same category as `always-comb` and the lifetime rules: SystemVerilog
advice reaching a Verilog-2001 design. Both are disabled in `.rules.verible_lint` with the
table above as the reason. **The substance was applied anyway** — every parameter and
localparam now carries an explicit width or `integer`, because knowing the width of a constant
is worth having whether or not a linter asks. That is now a project rule in
[AGENTS.md](../AGENTS.md) §5.3, enforced by review rather than by tooling, and the plan says so
rather than pretending the gate covers it.

**Enabling `explicit-begin` reached further than the six `if`/`else` bodies.** The rule also
covers `initial`, `always`, `for` and `while`, and it cannot be configured to check only `if`.
That is 19 further sites, all in the testbenches — `initial clock = 1'b0;`,
`always #12.5 clock = ~clock;`, the `for (…) @(posedge clock);` waits, and the
pass/fail `$display` pairs. All were given `begin`/`end` rather than the rule being narrowed:
the collapse problem applies to the testbenches' `if (errors == 0) … else …` exactly as it did
to `spiRegisters.v`.

**Converting the testbench constants from `parameter` to `localparam` exposed a rule
disagreement.** Verible's `parameter-name-style` accepts `UpperCamelCase` or `ALL_CAPS` for a
`parameter`, but only `UpperCamelCase` for a `localparam` — so `RAMP_LENGTH`, `PACKET_WORDS`,
`HALF_BIT`, `CS_TIME`, `COMMIT_TEXT` and `BUILD_FLAGS` began failing the moment they became
localparams, contradicting §1.2's "testbench constants: ALL_CAPS permitted". The localparam
half is widened to `CamelCase|ALL_CAPS` in the config. Only that half: a `lowerCamelCase` or
`snake_case` constant is still reported, which is what caught `bufferSize` and `commitText` in
the first place.

*Verification:* all four scripts pass; `run-style.sh` reports zero findings over all eight
files. `.jic` comparison as in Phase 1 — these edits are expected to be inert, and the
explicit widths are exactly the claim that check tests.

### Phase 5 — Enforcement

Only now, with the tree clean, does the gate become mandatory.

1. **`fpga/checks.nix`** — add a `style` attribute beside `lint`, `sim`, `provenance` and
   `version`, following the file's existing rule that *each check runs the same script a
   developer runs, rather than reimplementing it in Nix, so the two cannot drift*:

   ```nix
   # T4 — style. verible-verilog-format --verify and verible-verilog-lint over the
   # five project-authored modules and the three testbenches.
   style = runCommand "ddd-fpga-style" { nativeBuildInputs = [ verible ]; } ''
     bash ${src}/tests/run-style.sh
     touch $out
   '';
   ```

   The `src` fileset gains `./.verible-format` and `./.rules.verible_lint`; `verible` joins the
   argument list.

2. **`flake.nix`** — `fpga-style = fpgaChecks.style;` alongside the existing four.

3. **`.github/workflows/build.yml`** — append `.#checks.x86_64-linux.fpga-style` to the `fpga`
   matrix entry's `attrs`, which currently names lint, sim and provenance. Free, cross-platform,
   cacheable — it meets every constraint the header of `checks.nix` sets out for a CI check.

4. **`AGENTS.md` §5.3** — rewritten. The current text ("Follow the existing style:
   `lowerCamelCase` signal names, `always @(posedge clock)`") is replaced with the convention
   table of §1.2, the four recorded deviations of §1.1, the two commands
   (`run-style.sh` to check, `run-format.sh` to fix), and the same "do not silence a finding
   without a reason" rule the Verilator waivers already carry.

5. **`fpga/README.md`** — a "Style" section under *Working on the gateware without Quartus*,
   giving the two commands and pointing at the style guide.

6. **`docs/content/development/editor-setup.md`** — a note that `verible-verilog-ls` (already
   in the dev shell) picks up `.rules.verible_lint` automatically, so an LSP-capable editor
   shows the same diagnostics the gate enforces.

7. **`.editorconfig`** — no change needed. Its `[*.{v,sv,svh,vh}] indent_size = 4` is already
   what the formatter is configured to produce; after Phase 1 the two agree for the first time.

8. **`AGENTS.md` §5.4 and `tools/check-licence-headers.sh`** — also no change needed, and that
   is the point of §1.3. This plan brings the gateware headers *into* conformance with a
   convention the repository already documents and already gates; it does not invent a second
   one. The only thing §5.3 gains is a pointer to §5.4 for the header, so a contributor reading
   the Verilog section is not left to guess.

*Verification:* `nix flake check` passes with the new check present. Deliberately introduce a
tab in a source file and confirm CI fails; revert.

#### What Phase 5 surfaced

**1. The config files have to be tracked by git or the check cannot see them.** The first
`nix build .#checks.x86_64-linux.fpga-style` failed at evaluation:

```
error: lib.fileset.unions: Element 3 (…/fpga/.verible-format) is a path that does not exist.
```

A flake's source is the *git* tree, so an untracked file is invisible to it however plainly it
sits in the working directory. `.verible-format`, `.rules.verible_lint` and `verible-waivers`
are new files, and until they are added they are exactly the sort of thing that gets left
behind. This is not a Nix quirk to work around — it is the check correctly refusing to run
against configuration that is not in the repository. **`git add` the three config files and
the two scripts before this phase can be called done.**

Worth stating because the failure mode without the check is worse than the build error: a
`lib.fileset.maybeMissing` would have made the check silently run with Verible's *default*
settings — four-space indentation and the naming rules quietly gone — and still pass.

**2. `fpga-version` was defined but never ran in CI.** The `fpga` matrix entry in
`build.yml` named `fpga-lint`, `fpga-sim` and `fpga-provenance`. `fpga-version` has been in
`flake.nix`'s `checks` since it was written, so `nix flake check` ran it on full builds, but
the path-filtered pull-request job did not — meaning a change to `generate-version.sh` on a PR
was covered by nothing. Fixed in the same line that adds `fpga-style`, with a comment saying
the list must track `flake.nix`, since there is no `fpga` package for the matrix to fall back
on.

**3. The editor-setup page said Verilog formatting was "available but not enforced".**
True when written, false the moment this check lands. Rewritten, along with the Emacs and
Helix language-server snippets, which lacked the `--rules_config_search` flag the Neovim one
already had — without it an editor shows Verible's default diagnostics rather than this
project's.

*Result:* `nix build .#checks.x86_64-linux.fpga-style` passes; the same build with a single
tab appended to `dataGenerator.v` fails with `NEEDS FORMATTING` and `[no-tabs]`, and passes
again on revert.

## 4. Acceptance criteria

- [x] `./fpga/tests/run-style.sh` reports zero findings over the five modules and three testbenches
- [x] `./fpga/tests/run-lint.sh` and `./fpga/tests/run-sim.sh` still pass, unchanged
- [x] `nix flake check` includes `fpga-style` and passes
- [x] The `fpga` CI matrix entry builds `fpga-style`
- [x] No old identifier from the Phase 3 table survives anywhere outside `docs/site/` and history
- [x] `AGENTS.md` §5.3 describes the style the tooling actually enforces
- [ ] Every project-authored file in `fpga/` carries the canonical header of §1.3 — four-space
      indent, no tabs, SPDX pair adjacent and in order
- [ ] The thirteen files listed in §1.3 read `SPDX-FileCopyrightText: 2018-2026 Simon Inns`;
      the nine 2026 files still read `2026`; the four MegaWizard IP files still have no header
- [ ] `generate-version.sh` emits a `version.vh` header matching §1.3, and `fpga-version` passes
- [ ] `tools/check-licence-headers.sh` passes with its long-form-GPL count unchanged
- [x] Every project-authored file in `fpga/` carries the canonical header of §1.3
- [x] The thirteen files of §1.3 read `2018-2026`; the nine 2026 files still read `2026`
- [x] `generate-version.sh` emits a `version.vh` header matching §1.3, and `fpga-version` passes
- [x] `tools/check-licence-headers.sh` passes with its long-form-GPL count unchanged (25)
- [x] `fpga/verilator-waivers.vlt` is unchanged in substance — this plan adds no new waivers
      and removes none; the only edits are the year and three signal names in its prose
- [x] The three config files and two scripts are tracked by git (Phase 5, finding 1)

**Outstanding — hardware, to be done once the plan is complete:**

- [ ] Phase 1 produced a byte-identical `.jic`
- [ ] Phases 3 and 4 passed the TESTING.md §5 capture-integrity run with zero sequence breaks

Phases 1 and 2 are comment-and-whitespace only and the `.jic` should be byte-identical for
both. Phases 3 and 4 changed identifiers, parameter widths and block structure; none of that
is *intended* to change behaviour, and the testbenches agree, but the `.jic` may legitimately
differ and a capture-integrity run is the evidence that counts.

## 5. One open item

**Module and file names are left in `lowerCamelCase`** (§1.1), which under a `lower_snake_case`
signal convention produces lines that read a little oddly:

```verilog
dataGenerator data_generator_0 (
    .reset_n(fx3_reset_n),
    ...
```

The alternative is renaming the modules and their files to `data_generator`, `fx3_state_machine`,
`spi_registers`, `buffer` and `domesday_duplicator`, which additionally touches
`DomesdayDuplicator.qsf` (five `VERILOG_FILE` lines and `TOP_LEVEL_ENTITY`), the `.qpf`, `.cof`
and both `.cdf` files, the artefact names `DomesdayDuplicator.sof`/`.jic`, `run-lint.sh`,
`run-sim.sh`, `package.nix`, `build-local.sh`, `bitstream-provenance.py`, the licence-header
exemption list, and the programming instructions in `fpga/README.md` and the documentation site.

**Recommendation: leave them.** The `.jic` and `.sof` filenames are what users download from
releases and what `bitstream-provenance.txt` names, so renaming the top-level entity is a
user-visible change for an internal tidiness gain. If it is wanted, it belongs in its own
change with its own hardware verification, not folded into this one.
