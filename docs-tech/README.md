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
| [agents-and-testing.md](agents-and-testing.md) | Plan for `AGENTS.md` and `TESTING.md` — agent rules, per-component test tiers, and the capture-integrity system test |

Read `reorganisation-plan.md` for the *what* and *why*; `implementation-plan.md` for the
*how* and in what order.

Status: **Phases 0, 1 and 2 complete.** Written 2026-08-12 against commit `bcd53a0`; the work
runs on branch `20260812-002`, and `master` is untouched until every phase is done.

| Phase | State |
| --- | --- |
| 0 — Decisions and spikes | Done. All seven decisions in [decisions.md](decisions.md) |
| 1 — Merge the submodules | Done. Four repositories folded in with history; no submodules remain |
| 2 — Re-layout and defect fixes | Done. Target directory structure in place; **D1–D8 and D13–D17 closed** |
| 3 — Nix foundation and easy flakes | Next |
| 4–8 | Not started |

Outstanding items carried forward:

- **P0-3 hardware verification** — a Phase 6 gate, not a blocker.
- **`fpgaupdate-202512` was not imported.** P0-1 was reversed: only the pinned submodule
  commits came in. That branch's Quartus 25.1 upgrade, hand-written Gray-code FIFO and
  `IPfifo_tb.v` testbench are still only in the old `firmware` repository, and would need
  re-applying as a patch series if wanted.
- **`.envrc`** is deferred from P2-13 to Phase 3, where the root flake it refers to exists.
