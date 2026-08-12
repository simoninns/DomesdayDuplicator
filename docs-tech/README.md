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

Status: **Phase 0 complete**. Written 2026-08-12 against commit `bcd53a0` (branch
`20260812-001`). No code has changed; Phase 0 produces decisions only, and all seven are
recorded in [decisions.md](decisions.md).

Two maintainer actions carry into Phase 1 prep: fast-forward the `firmware` submodule onto
`fpgaupdate-202512` (P0-1), and refresh the Cypress SDK from the vendor download (P0-2,
placement instructions in the decision log). P0-3's hardware verification is a Phase 6 gate.
