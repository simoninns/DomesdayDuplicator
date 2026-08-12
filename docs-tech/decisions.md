# Decision log

Phase 0 of [implementation-plan.md](implementation-plan.md). Each entry records the evidence
gathered, the options, a recommendation, and the decision once made.

**Status key:** `PENDING` — awaiting the maintainer. `DECIDED` — settled, with date.
No later phase may start until every entry below is `DECIDED`.

Evidence gathered 2026-08-12 against `bcd53a0` (branch `20260812-001`).

| | Decision | Status |
| --- | --- | --- |
| P0-1 | Outstanding branches | **DECIDED (revised)** — import at the **pinned commits**; `fpgaupdate-202512` and `release-2.x` both left behind |
| P0-2 | Cypress SDK | **DECIDED** — vendor it regardless; refresh from the vendor download |
| P0-3 | Quartus version | **DECIDED** — accept 25.1; but the 25.1 work was **not imported** (see P0-1 revision), so the branch must be re-applied first |
| P0-4 | Docs site URL | **DECIDED** — move, no redirect |
| P0-5 | History size | **DECIDED** — accept ~400 MB, no pruning |
| P0-6 | Old repositories | **DECIDED** — leave alone, cleaned up separately |
| P0-7 | macOS support | **DECIDED** — best-effort, not a CI gate |

**Phase 0's decisions are complete.** Only P0-3's hardware verification remains outstanding,
and it is a Phase 6 gate rather than a blocker — Phase 1 can begin.

---

## P0-1 — Freeze window and outstanding branches

**Question:** which unmerged branches land before the monorepo merge, and which are abandoned?
Rebasing a branch across a history rewrite is painful, so this must be settled first.

### Evidence

**`firmware` → `origin/fpgaupdate-202512`** — **4 ahead, 0 behind `master`** (a clean
fast-forward). Contents:

| Commit | |
| --- | --- |
| `c9091d3` | Upgraded FPGA project for Quartus Prime Lite 25.1 |
| `8b3a801` | Fix warnings about tri-state ports |
| `5934d9e` | Just checkpointing the code (need to test) |
| `cb99341` | 25.1 first test version of the new FPGA FIFO code |

This branch is far more significant than the plan assumed. It:

- Sets `LAST_QUARTUS_VERSION "25.1std.0 Lite Edition"` in the `.qsf` — **the 25.1 upgrade is
  already done**, which is most of P0-3
- **Replaces the Intel `dcfifo` IP with a hand-written parameterised dual-clock FIFO**
  (Gray-code clock-domain crossing, SPDX headers, GPL-3.0-or-later), switching the `.qsf`
  entry from `QIP_FILE IPfifo.qip` to `VERILOG_FILE IPfifo.v` and deleting `IPfifo_bb.v`
- **Adds `IPfifo_tb.v`** — a 333-line testbench covering write/read, clock-domain crossing,
  empty/full flags, used-words counters, show-ahead mode and asynchronous clear
- Regenerates `IPpllGenerator` under 25.1 (`IP_TOOL_VERSION` 18.0 → 25.1). The PLL **still
  uses the vendor `altpll` megafunction** — it is the only vendor IP left after this branch
- Cleans up commented-out `GPIO1_IN` assignments and updates `statusLED.v`

Last commit message says "need to test", so it is unverified on hardware.

**superproject → `origin/release-2.x`** — 26 ahead, 109 behind. Diverged 2022-06-19, last
commit 2022-12-26. It is on the **pre-submodule-split flat layout** (`DE0-NANO/`,
`FX3-Firmware/`, `KiCAD/`, `Linux-Application/`, `Documentation/`, `Graphics/`), so it cannot
be merged into the current structure in any meaningful sense. Spot-check: its Windows
page-lock work (`VirtualLock`) is already present in `master` at
`gui-app/tools/DomesdayDuplicator/UsbDeviceBase.cpp:1125`.

### Recommendation

- **`fpgaupdate-202512`: land it before Phase 1.** It fast-forwards, and it is the answer to
  P0-3. Landing it first means the monorepo merge captures the 25.1 work rather than
  stranding it behind a rewritten history.
- **`release-2.x`: leave it alone.** It is a 2022 historical release branch on a layout that
  no longer exists, and its content is already in `master`. Optionally tag it
  `release-2.x-final` for clarity and delete the branch, but no merge is possible or wanted.

### Decision

**`DECIDED` 2026-08-12 — land `fpgaupdate-202512` before Phase 1; leave `release-2.x`
untouched.**

Consequences:

- Landing the FPGA branch is a **pre-Phase-1 action**, not part of the merge PR. Fast-forward
  `firmware`'s `master` to `origin/fpgaupdate-202512`, push, then update the superproject's
  submodule pointer before Phase 1 begins.
- `release-2.x` stays as-is on the superproject. It is neither tagged nor deleted, and after
  the monorepo merge it will point at a history that no longer resembles the trunk. That is
  accepted; it is a historical artefact.
- The 25.1 work lands **unverified on hardware** (its last commit says "need to test"), so
  P0-3's hardware verification becomes a Phase 6 gate rather than a Phase 0 blocker.

### Amendment 2 (2026-08-12) — REVERSED: import at the pins, FPGA branch left out

**This supersedes the decision above.** The maintainer chose to import all four submodules
**exactly as pinned by the superproject**, ignoring every commit beyond the pin — including
`fpgaupdate-202512`.

Imported commits:

| | Pinned SHA | Notes |
| --- | --- | --- |
| `docs` | `9e57a729` | 2 commits behind `origin/main` — those are not imported |
| `firmware` | `83a98bbc` | **Without** the Quartus 25.1 work |
| `gui-app` | `8036eaf1` | 1 commit behind `origin/master` — not imported |
| `hardware` | `76099311` | at tip |

Consequences:

- **The Quartus 25.1 gateware work is not in the monorepo.** It remains on
  `fpgaupdate-202512` in the old `DomesdayDuplicator-firmware` repository, which P0-6 leaves
  in place, so nothing is lost. Bringing it over later means applying it as a patch series —
  its four commits' branch history would not survive, but the content will.
- **P0-3 is affected.** The monorepo's FPGA project is the 18.0-era one again: `.qsf` says
  `LAST_QUARTUS_VERSION "18.0.0 Lite Edition"`, `IPfifo` is the Intel `dcfifo` IP via
  `IPfifo.qip`, and `IPfifo_tb.v` is absent. So the Quartus-25.1 decision now needs the
  branch re-applying before it is true of this repository.
- **P6-7 loses its head start.** The existing testbench came with that branch; gateware
  testbench work starts from nothing again.
- Three merged pull requests (2 in `docs`, 1 in `gui-app`) are likewise not imported.

### Amendment 1 (2026-08-12) — import scope

**Only each submodule's default branch is imported into the monorepo.** No other branches,
and no tags.

Note the default branch name is not uniform: `docs` uses **`main`**; `firmware`, `gui-app`
and `hardware` use **`master`**. "Only main" is therefore implemented as "only the default
branch of each repository", which is what the four `--branch` arguments in
[submodule-migration.md](submodule-migration.md) §3.1 name.

Consequences:

- Clones for the import use `--single-branch --no-tags`, so no other branch's history enters
  the monorepo at all — a smaller, cleaner result than filtering afterwards.
- `fpgaupdate-202512` is preserved only because P0-1 lands it into `firmware`'s `master`
  first. Any branch *not* landed before Phase 1 stays behind in the old repository, which
  P0-6 leaves in place — so nothing is destroyed, but nothing else is carried across either.
- The `pre-monorepo` tags are still pushed to the old repositories; they simply are not
  imported.

---

## P0-2 — Cypress redistribution review

**Question:** may the vendored CyFX3 SDK, and `cyfxflashprog.img`, be redistributed in a
public repository?

### Evidence

**This is the highest-priority Phase 0 item.** The vendored SDK headers carry an explicit
proprietary notice. From `fx3/sdk/fw_lib/1_3_5/inc/cyu3error.h` (representative of the set):

```
##  Copyright Cypress Semiconductor Corporation, 2010-2023,
##  All Rights Reserved
##  UNPUBLISHED, LICENSED SOFTWARE.
##
##  CONFIDENTIAL AND PROPRIETARY INFORMATION
##  WHICH IS THE PROPERTY OF CYPRESS.
##
##  Use of this file is governed
##  by the license agreement included in the file
##     <install>/license/license.txt
```

Facts, stated plainly:

1. The notice says *All Rights Reserved*, *UNPUBLISHED*, *CONFIDENTIAL AND PROPRIETARY*.
2. The licence agreement it points at — `<install>/license/license.txt` — **is not in the
   repository**. There is no licence file anywhere under `cyfx3sdk/`.
3. The SDK is already published in a public GitHub repository, and has been for some time.
4. `firmware/cyfxtx.c`, which is compiled into the project's own firmware, carries the same
   Cypress notice.

Two mitigating points, offered for balance rather than as reassurance:

- `cyfxtx.c`'s own comment states *"This file shall be provided in source form and must be
  compiled with the application source code"* — Cypress evidently intended that file to ship
  with applications.
- Vendor SDKs frequently carry boilerplate of this severity while their actual EULA permits
  exactly this kind of redistribution for use with the vendor's silicon. The notice is not
  the licence.

But the governing document is absent, so the position cannot currently be verified either
way — and the plan makes this repository more prominent.

Separately, `fx3-programmer.c` states it is derived from Cypress `cyusb_linux` yet carries
**no licence or copyright header at all**, which is a compliance gap regardless of the EULA's
terms.

### Decision

**`DECIDED` 2026-08-12 — vendor the SDK in the repository regardless of the licence review.**
Maintainer's call, on the basis that the SDK is already widely mirrored on GitHub. The
maintainer will refresh it from the official vendor download.

Consequences, all simplifying:

- **P0-2 no longer blocks Phase 1 or Phase 5.** No history rewrite is needed, which keeps
  P0-5's "accept ~400 MB" decision clean.
- The `requireFile` fallback design in
  [implementation-plan.md](implementation-plan.md) is retained as documentation only. It is
  not the path taken, but costs nothing to leave on the page should the position ever change.
- The FX3 firmware build stays in `nix flake check` and in CI.
- P5-3 becomes "record provenance", not "decide the mechanism".
- The notices quoted above remain factually on the files. Ship the vendor's own
  `license/license.txt` alongside them (§ below) so anyone reading the headers can find the
  document they point at.

### Outcome of the refresh (2026-08-12)

The maintainer supplied `ezusbfx3sdk_1.3.5_Linux_x32-x64.tar.gz` (445 MB). It contains five
inner archives: `fx3_firmware_linux.tar.gz` (the SDK), `cyusb_linux_1.0.5.tar.gz` (host
tools), `ARM_GCC.tar.gz`, and Eclipse for x86 and x64. Only the first two were extracted —
the toolchain comes from nixpkgs and the project does not depend on an IDE.

Findings:

1. **The vendored SDK was already pristine.** `diff -rq` against the freshly extracted
   `cyfx3sdk` shows every shared file byte-identical. The only differences are the three
   unused library profiles (present in the repo, not extracted) and the project-authored
   `util/elf2img/CMakeLists.txt`. So the refresh was a **no-op for SDK content** — its value
   was confirming provenance, which is now recorded in `cyfx3sdk/README.md`.
2. **The extracted subset is provably sufficient.** Configured and built the firmware against
   it with `gcc-arm-embedded` from nixpkgs: `firmware.elf`, `firmware.map` and a 111 KB
   `firmware.img` all produced. So `inc/` + `fx3_release/` + `fx3.ld` + `elf2img` is the
   complete required set.
3. **The archive contains no SDK licence file.** The `cyfx3sdk` tree holds only three PDFs at
   its root; the `<install>/license/license.txt` referenced by every header is generated by
   the vendor's installer, not shipped in the tarball. There is therefore nothing to vendor
   alongside the headers, and the earlier plan to do so cannot be carried out.
4. **`cyusb_linux` is LGPL-2.1** — its `license.txt` is the full LGPL 2.1 text. That covers
   both `cyfxflashprog.img` and the `fx3-programmer.c` derivation, and LGPL-2.1 §3 permits
   relicensing under the GPL, so it sits comfortably under this project's GPLv3.
5. **D13's missing file was in the archive.** `cyfxflashprog.img` is now vendored — see
   below.

The archive was deleted after extraction, at the maintainer's request.

### Files placed

| Path | From | Notes |
| --- | --- | --- |
| `firmware/fx3/fx3-programmer/cyfxflashprog.img` | `cyusb_linux_1.0.5/fx3_images/` | **Resolves D13's missing artefact.** 106,456 bytes, SHA-256 `818fff4f…30c30` |
| `firmware/fx3/fx3-programmer/cyfxflashprog.txt` | same | Vendor description of the loader's command protocol |
| `firmware/fx3/fx3-programmer/LICENSE.cyusb_linux.txt` | `cyusb_linux_1.0.5/license.txt` | LGPL-2.1 |
| `firmware/fx3/fx3-programmer/VENDOR.md` | written | Provenance, checksums, and how the loader is located |
| `firmware/fx3/fx3-firmware/cyfx3sdk/README.md` | written | SDK provenance and the two refresh traps |

Placing `cyfxflashprog.img` at the programmer's directory root is deliberate: the existing
`find_flashprog_image()` searches `../cyfxflashprog.img`, which resolves when the tool is run
from `build/`. So flash programming works today with no code change. **P2-10 is still
required** for installed binaries, since every current candidate path is
working-directory-relative.

### Refreshing the SDK in future — where to put it

The vendor archive is `FX3_SDK_1.3.5_Linux.tar.gz` (or the Windows installer's equivalent
tree). Only the `cyfx3sdk` subtree is needed.

**Target, before the Phase 2 re-layout:**

```
firmware/fx3/fx3-firmware/cyfx3sdk/
```

**Target, after P2-1:**

```
fx3/sdk/
```

This path is not arbitrary — `CMakeLists.txt` defaults `CYFX3SDK_PATH` to
`${CMAKE_CURRENT_SOURCE_DIR}/cyfx3sdk` and derives everything from it.

**Required contents** — the build fails with an explicit `FATAL_ERROR` if any of the first
three are missing:

| Path under `cyfx3sdk/` | Contents | Needed |
| --- | --- | --- |
| `fw_lib/1_3_5/inc/` | 47 headers | **Required** — `CYFX3SDK_INCLUDE_DIR` |
| `fw_lib/1_3_5/fx3_release/` | `libcyfxapi.a`, `libcyu3threadx.a`, `libcyu3lpp.a` | **Required** — `CYFX3SDK_LIB_DIR`. These three are the only ones linked |
| `fw_build/fx3_fw/fx3.ld` | Linker script | **Required** — `CYFX3SDK_LINKER_SCRIPT` |
| `util/elf2img/elf2img.c` | Host tool source | **Required** — builds the `.img` from the `.elf` |
| `fw_lib/1_3_5/fx3_release/libcy_as0260.a`, `libcy_ov5640.a`, `libcyu3mipicsi.a`, `libcyu3sport.a` | Image-sensor, MIPI-CSI and serial-port libraries | Unused (~1.9 MB). Harmless to keep |
| `fw_lib/1_3_5/fx3_debug/`, `fx3_profile_debug/`, `fx3_profile_release/` | Debug/profiling variants | Unused (~45 MB). P2-8 removes them |

**Two things to preserve when overwriting with the pristine SDK:**

1. **`util/elf2img/CMakeLists.txt` is project-authored, not vendor.** It was added in
   `19bdb88` ("Restructure FX3 firmware and implement minimal programmer tool") to replace
   the vendor's Makefile. Copying the SDK's `util/elf2img/` over the top would delete it and
   break the firmware build. Keep it, or restore it afterwards.
2. **The version string is embedded in the path.** `CMakeLists.txt` sets
   `CYFX3SDK_VERSION "1_3_5"`, so the library directory must be literally
   `fw_lib/1_3_5/`. If the download is a different version, rename to match *or* update that
   variable — do not leave the two disagreeing.

~~**Also copy** `<install>/license/license.txt`.~~ **Not possible** — finding 3 above: the
Linux archive contains no such file. It is created by the vendor's installer. If you ever run
that installer, copying the generated `license/license.txt` into `fx3/sdk/` would be worth
doing.

`fx3/sdk/README.md` recording version, origin and date is **done** — see
`firmware/fx3/fx3-firmware/cyfx3sdk/README.md`, which moves to `fx3/sdk/README.md` at P2-1.

### Related

- **`cyfxflashprog.img` (D13)** — the file itself is now vendored (see above). The *code*
  half of D13 remains: `find_flashprog_image()` only searches working-directory-relative
  paths, so installed binaries still cannot locate it. P2-10 and P3-3 stand.
- **`fx3-programmer.c` has no copyright or licence header** despite being a `cyusb_linux`
  derivative (D20). Now better understood: the upstream is **LGPL-2.1**, which permits the
  relicensing to GPLv3 this project implies. Fold into P8-5's SPDX rollout.

---

## P0-3 — Quartus version

**Question:** accept Quartus Prime Lite 25.1 (what nixpkgs ships), or pin an older nixpkgs
for 18.0?

### Evidence

Largely answered by `fpgaupdate-202512` (P0-1): the upgrade to 25.1 has been done, the FIFO
IP replaced with portable Verilog, and a testbench written. What remains is verification.

Supporting facts:

- nixpkgs `quartus-prime-lite` is 25.1, `x86_64-linux` only, unfree,
  `redistributable = false`; it accepts `supportedDevices` (need only `"Cyclone IV"`) and
  `withQuesta` (set `false`).
- Pinning an old unfree installer is fragile: Altera withdraws old downloads, and a stale
  fixed-output hash then makes the flake unbuildable with no local remedy.
- After `fpgaupdate-202512`, the only vendor IP left is `altpll` in `IPpllGenerator.v`.

### Recommendation

**Accept 25.1.** Land `fpgaupdate-202512`, then verify on hardware using the
capture-integrity procedure ([agents-and-testing.md](agents-and-testing.md) §4) before
retiring the 18.0-built bitstream. Keep the released `.jic` in-tree until that passes.

### Decision

**`DECIDED` 2026-08-12 — accept 25.1.** The upgrade already exists on `fpgaupdate-202512`,
which P0-1 lands before Phase 1.

Outstanding: hardware verification, since that branch's last commit says "need to test". This
is a **Phase 6 gate (P6-5)**, not a blocker on anything earlier — run the capture-integrity
procedure, and keep the released 18.0-built `.jic` in-tree until it passes.

---

## P0-4 — Documentation site URL

**Question:** where does the documentation site live after docs folds into the monorepo, and
does the old URL get a redirect?

### Evidence

- `_config.yml` has `baseurl: "/DomesdayDuplicator-docs"`, `url: "https://simoninns.github.io"`.
- **No `CNAME` file anywhere** in the docs tree — so there is no custom domain today, and the
  site is served at `simoninns.github.io/DomesdayDuplicator-docs`.
- Inbound links needing update: `README.md:3`, `README.md:37` (a deep link to
  `Related-Projects/The-ld-decode-Family.html`), `docs/README.md:7`, `CONTRIBUTING.md:13`.
- The MkDocs conversion changes per-page URL shape as well (`…/Page.html` → `…/page/`), so
  deep links break regardless of the move.
- An archived GitHub repository keeps serving its last Pages build but cannot rebuild, so
  publishing a redirect stub means briefly un-archiving.

### Options

| | Destination | Redirect stub at the old URL |
| A | `simoninns.github.io/domesdayduplicator` | Yes — un-archive docs repo, publish a redirecting `index.html`, re-archive |
| B | `simoninns.github.io/domesdayduplicator` | No — accept that old links 404 |
| C | Custom domain (e.g. `docs.domesdayduplicator.org`) | Separate question; insulates against any future move |

### Recommendation

**A.** The redirect is a few minutes' work during the Phase 1 archiving step, and the docs
site is the project's main public entry point. C is worth considering independently, since a
custom domain would have prevented this problem entirely.

### Decision

**`DECIDED` 2026-08-12 — option B: move to `simoninns.github.io/domesdayduplicator`, no
redirect stub.**

Consequences:

- Existing inbound deep links to `simoninns.github.io/DomesdayDuplicator-docs/…` will 404.
  Known external references (ld-decode community pages, forum posts) are not under this
  project's control and will not be updated.
- P4-8 is simplified: set `site_url`, fix the four in-repo links (`README.md:3`,
  `README.md:37`, `docs/README.md:7`, `CONTRIBUTING.md:13`), and nothing else.
- No un-archive/re-archive dance is needed for the docs repository, which also aligns with
  P0-6's decision to leave the old repositories alone entirely.
- Reversible later if desired: a redirect stub can be added at any point, since the old repo
  is being left in place rather than deleted.

---

## P0-5 — Repository history size

**Question:** accept the merged repository's size, or strip large blobs during the Phase 1
rewrite? Irreversible, and only possible *during* Phase 1.

### Evidence

Merged history will be roughly 400 MB (root 38 MB of objects + 174 MB of submodule history,
plus merge overhead). Contributors already pay this with `git clone --recursive`.

`docs` dominates at 140 MB of history for 19 commits. Largest blobs:

| Size | Path |
| --- | --- |
| 7.9 MB | `Misc/assets/DdD-Fab/DdD-New-PCBWay-Fab-…12.01.47.jpg` |
| 6.8 MB | `Support/assets/DatorMagazinRetro3Domesday.pdf` |
| 4.7 MB | `Misc/assets/DdD_Shielded_Case_…08.50.34.jpg` |
| 4.1 MB | `Hardware/assets/Domes-Day-Duplicator-Rev3-Trasparent-ver-harrypm.png` |
| 3.9 MB | `Misc/assets/DdD-Black-PCB.png` |
| 2.9 MB | `Unused-Assets/image2.png` |
| 2.6 MB | `Unused-Assets/DdD-Green-PCB.png` |

Note two of the top seven are in `Unused-Assets/`, which P4-5 deletes from the checkout
anyway — deleting from the checkout does **not** shrink history.

`firmware` contributes 26 MB, including ~45 MB of unreferenced SDK library profiles in the
working tree (`fx3_debug`, `fx3_profile_debug`, `fx3_profile_release`), which P2-8 removes
from the checkout only.

### Options

| | Action | Cost |
| A | Accept ~400 MB | None. Existing contributors see no change |
| B | Strip blobs > 5 MB from `docs` history during the rewrite | Old revisions lose those images; every `docs` SHA changes; irreversible |
| C | Also strip the unused SDK profiles from `firmware` history | Same, plus interacts with P0-2 |
| D | Git LFS | A hard dependency for every contributor, and no help to GitHub Pages, which needs the real files |

### Recommendation

**A**, unless P0-2 forces a rewrite anyway — in which case do C at the same time, since the
expensive part (rewriting and force-pushing) is already being paid for.

### Decision

**`DECIDED` 2026-08-12 — option A: accept ~400 MB. No history pruning, no LFS.**

Consequences:

- Phase 1 uses `git filter-repo --to-subdirectory-filter` **only** for path prefixing. No
  `--strip-blobs-bigger-than`, no blob filtering.
- P2-8 (prune unused SDK library profiles) and P4-5 (delete `Unused-Assets/`) still go ahead
  — they shrink the *checkout*, which is the part contributors interact with daily. History
  keeps the blobs.
- **Caveat carried forward:** if P0-2 concludes the Cypress SDK cannot be redistributed, the
  blobs must come out of history, which is a rewrite. That rewrite can only happen during
  Phase 1. So **P0-2 must be answered before Phase 1 starts**, not merely before Phase 5.

---

## P0-6 — Disposition of the four upstream repositories

**Question:** archive or delete `DomesdayDuplicator-{hardware,firmware,gui-app,docs}`?

### Evidence

Archiving preserves issue history, stars, forks and inbound links, and keeps the last Pages
build serving (relevant to P0-4). Deleting breaks every existing link and clone URL, and
frees nothing of value. Three of the four are also referenced by SSH URLs in `.gitmodules`,
so anyone with an existing recursive clone has a remote pointing at them.

### Recommendation

**Archive**, with a notice at the top of each README pointing at the monorepo and naming the
`pre-monorepo` tag. Sequence the docs repo's archiving with P0-4's redirect stub so it is not
un-archived twice.

### Decision

**`DECIDED` 2026-08-12 — leave the old repositories alone. Neither archived nor deleted; the
maintainer will clean them up separately, outside this plan.**

Consequences:

- **P8-1 is removed from the plan.** No archiving step, no README notices, no repository
  settings changes.
- The `pre-monorepo` tags in Phase 0 prep are still worth pushing — they cost nothing and
  mark the handover point cleanly for whenever the cleanup does happen.
- Until that cleanup, the four repositories remain **writable**, and their READMEs will not
  mention the monorepo. Contributors and existing recursive clones may keep pushing to them,
  and any such work will need porting by hand. Worth a short note in the top-level README
  during P2-9 so the monorepo is unambiguously the place to work.

---

## P0-7 — macOS support level

**Question:** is a Nix Darwin build of the GUI a CI gate, or best-effort?

### Evidence

macOS is genuinely supported and actively maintained today, but **not via Nix**. The existing
GUI workflow has two macOS jobs — `build-macos-x64` on `macos-15-intel` and
`build-macos-arm64` on `macos-latest` — both building with Homebrew (`brew install cmake
pkg-config libusb qt6`). The runner label carries the comment "macOS 15 Intel runner
(macos-13 retired)", so someone has maintained these recently.

Gating `nix flake check` on Darwin outputs would require a macOS runner in the Nix workflow,
which the project does not have and does not need — the Homebrew jobs already provide real
macOS coverage of the thing users actually download.

### Recommendation

**Best-effort.** Expose `packages.aarch64-darwin.gui` so Nix-on-macOS developers can use it,
but do not gate CI on it. Keep the Homebrew macOS jobs as the authoritative macOS coverage
(P7-2 already preserves them). Revisit only if someone actually develops on Nix under macOS.

### Decision

**`DECIDED` 2026-08-12 — best-effort, taken as the default.** Low-stakes and trivially
reversed: gating on Darwin later needs only a macOS runner added to the Nix workflow. Flag it
if you would rather Nix-on-macOS be a first-class supported path.

---

## Incidental findings from this evidence pass

Not decisions, but recorded here so they are not lost:

- **D19** — `fx3/programmer/src/fx3-programmer.c` line 4 has a stray line of code sitting
  inside the file header comment block (`    int fd, ret, bytes_sent = 0;`). Harmless, since
  it is inside `/* … */`, but plainly an editing accident. Fix in P2-9.
- **D20** — `fx3-programmer.c` has no copyright or licence header despite being a derivative
  of Cypress `cyusb_linux`. Fold into P0-2's follow-up and P8-5's SPDX rollout.
- A gateware testbench (`IPfifo_tb.v`) already exists on `fpgaupdate-202512`, so **P6-7 starts
  from one working example rather than from nothing**, and D18's "no tests anywhere" is true
  of `master` but not of that branch.
