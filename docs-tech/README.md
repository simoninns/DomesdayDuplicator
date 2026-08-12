# docs-tech

Technical / engineering-process documentation for the Domesday Duplicator repository
itself — as opposed to `docs/`, which is the user-facing project website.

Nothing in here is published to GitHub Pages.

| Document | Contents |
| --- | --- |
| [reorganisation-plan.md](reorganisation-plan.md) | The main plan: current state, target layout, phases, risks, open questions |
| [decisions.md](decisions.md) | Phase 0 decision log — evidence, options and outcomes; includes where to place the refreshed Cypress SDK |
| [implementation-plan.md](implementation-plan.md) | Task-level execution: numbered tasks, gates, the D1–D12 defect register, tool acquisition strategy |
| [nix-flake-design.md](nix-flake-design.md) | Per-component flake design, including the Quartus and Cypress SDK problem cases |
| [submodule-migration.md](submodule-migration.md) | Exact git commands for folding the four submodules back into the monorepo with history |
| [docs-theme-migration.md](docs-theme-migration.md) | Phase 4 in full: converting the documentation site from Jekyll to MkDocs Material, matching decode-orc |
| [ide-independence.md](ide-independence.md) | Keeping the project free of Quartus-GUI and Eclipse dependencies: CLI flows, LSP for any editor |
| [editor-setup.md](editor-setup.md) | The practical follow-through: per-editor setup for VS Code, Neovim, Emacs, Helix, Qt Creator, CLion and KDevelop |
| [agents-and-testing.md](agents-and-testing.md) | Plan for `AGENTS.md` and `TESTING.md` — agent rules, per-component test tiers, and the capture-integrity system test |

Read `reorganisation-plan.md` for the *what* and *why*; `implementation-plan.md` for the
*how* and in what order.

Status: **Phases 0–4 complete.** Written 2026-08-12 against commit `bcd53a0`; the work runs
on branch `20260812-002`, and `master` is untouched until every phase is done.

| Phase | State |
| --- | --- |
| 0 — Decisions and spikes | Done. All seven decisions in [decisions.md](decisions.md) |
| 1 — Merge the submodules | Done. Four repositories folded in with history; no submodules remain |
| 2 — Re-layout and defect fixes | Done. Target directory structure in place; **D1–D8, D13–D17 closed** |
| 3 — Nix foundation and easy flakes | Done. `nix flake check` passes; **D18, D19 closed**; 44 tests where there were none |
| 4 — docs: Jekyll → MkDocs Material | Done. Site builds under `--strict`; **D9, D10, D12, D20 closed** |
| 5 — fx3 firmware flake | Next |
| 6 — fpga flake (Quartus) | Not started |
| 7 — CI consolidation | Not started |
| 8 — Cleanup | Not started |

**All twenty registered defects are now closed**, though two carry hardware checks that
cannot be done here (D13 in Phase 5, P0-3 in Phase 6).

Outstanding items carried forward:

- **P0-3 hardware verification** — a Phase 6 gate, not a blocker.
- **P3-3's NixOS half is unverified.** The udev module evaluates and the rule installs to the
  right place, but "a plugged-in FX3 gets the expected permissions, and a flash operation
  completes from the Nix-installed binary" needs real hardware and a NixOS rebuild.
- **`docs/LICENSE` contradicts the site's stated licence.** It is a GPLv3 copy inherited
  from the docs submodule; the site footer says content is CC BY-SA 4.0. Left for the
  maintainer to decide, and noted in `docs/README.md`.
- **`fpgaupdate-202512` was not imported.** P0-1 was reversed: only the pinned submodule
  commits came in. That branch's Quartus 25.1 upgrade, hand-written Gray-code FIFO and
  `IPfifo_tb.v` testbench are still only in the old `firmware` repository, and would need
  re-applying as a patch series if wanted.

**Inbound documentation links break once, now**, as designed — the site moved to
`simoninns.github.io/domesdayduplicator` and MkDocs uses directory URLs. Per P0-4 there is no
redirect stub.
