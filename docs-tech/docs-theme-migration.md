# Documentation: Jekyll → MkDocs Material

Aligning the Domesday Duplicator documentation site with the one used by
[decode-orc](https://github.com/decode-orc/decode-orc), which is checked out locally at
`/home/sdi/Coding/decode-orc`.

This is [implementation-plan.md](implementation-plan.md) **Phase 4** in full. It replaces the
"package the existing Jekyll site with `bundlerEnv`" approach in earlier drafts of these
documents — converting is *less* work than packaging what is there now, and it deletes two
defects rather than working around them.

## 1. What decode-orc uses

Read from `/home/sdi/Coding/decode-orc/mkdocs.yml`, `flake.nix` and
`.github/workflows/deploy-docs.yml`:

| Layer | Choice |
| --- | --- |
| Generator | MkDocs |
| Theme | `material` (Material for MkDocs), `indigo` primary/accent, light + dark `slate` toggle |
| Nav | `mkdocs-awesome-nav` — navigation derived from directory structure, ordered by per-directory `.nav.yml` |
| Search | Material's built-in `search` plugin |
| Extensions | `pymdownx.highlight` (with `anchor_linenums`), `pymdownx.superfences`, `pymdownx.tasklist`, `pymdownx.details`, `admonition`, `tables`, `attr_list`, `md_in_html`, `toc` with `permalink` |
| Styling | `extra_css: [assets/custom.css]` |
| Build | Nix: `python312.withPackages [ mkdocs mkdocs-material mkdocs-awesome-nav ]`, `mkdocs build`, install `site/*` to `$out` |
| Deploy | GitHub Actions: install Nix → `nix build .#docs` → `upload-pages-artifact` with `path: ./result` → `deploy-pages` |

All three Python packages are in nixpkgs today — `mkdocs` 1.6.1, `mkdocs-material` 9.7.6,
`mkdocs-awesome-nav` 3.3.0 — so nothing needs vendoring, and unlike the Ruby route there is
no `Gemfile`/`gemset.nix` to author or keep in sync.

## 2. Why this is the right call for DdD too

The current site is Jekyll with `remote_theme: just-the-docs/just-the-docs`, deployed by
`actions/jekyll-build-pages`. Converting resolves, rather than works around, two known
defects:

- **D9** — `remote_theme` clones the theme from GitHub *at build time*, which cannot work in
  a Nix sandbox. Material comes from nixpkgs, so the problem disappears instead of needing a
  `Gemfile` + `bundix` + a config change to `theme:`.
- **D12** — `build-local.sh` injects front matter that `_config.yml` already supplies via
  `defaults:`, and its error message still names `jekyll-theme-cayman`, a theme the site
  stopped using. `mkdocs serve` replaces the script entirely.

It also means the Nix-built site and the deployed site are **the same derivation**, which the
Jekyll route could never quite guarantee (a `bundlerEnv` Jekyll is not byte-identical to
GitHub's `github-pages` gem set). decode-orc's workflow builds with Nix and uploads the
result directly; DdD gets the same property for free.

And the two projects then share one documentation toolchain, which matters given how much
content moves between them.

## 3. Scope of the conversion — smaller than it looks

Surveyed across all 24 markdown files in `docs/wiki-default/`:

- **Zero files have YAML front matter.**
- **Zero files contain Liquid** (`{%` / `{{`).
- No `just-the-docs` front matter (`nav_order`, `parent`, `has_children`) anywhere.

All the Jekyll-specific machinery is confined to files that get **deleted**:
`_layouts/default.html` (22 Liquid tags), `_includes/sidebar-nav.html` and
`_includes/footer-nav.html` (8 each), `_config.yml`, `Sidebar.md`, `Footer.md`,
`search.json`.

So the markdown itself carries over essentially untouched. The real work is navigation and
the URL move.

### 3.1 Files: delete / move / create

| Action | Files |
| --- | --- |
| **Delete** | `_layouts/`, `_includes/`, `_config.yml`, `Sidebar.md`, `Footer.md`, `search.json`, `build-local.sh`, `check-internal-linkage.sh`, `check-orphans.sh`, `show-external-links.sh` |
| **Delete (6 MB)** | `Unused-Assets/` — it is unreferenced by name, and MkDocs copies everything under `docs_dir` into the site. If it must be kept, use `exclude_docs:` rather than letting it ship |
| **Move** | The 24 `.md` files and their per-folder `assets/` directories, plus `favicon.ico` |
| **Create** | `docs/mkdocs.yml`, `docs/package.nix`, one `.nav.yml` per content directory, `docs/content/assets/custom.css` |

The three shell scripts are replaced by **`mkdocs build --strict`**, which fails the build on
broken internal links — the job `check-internal-linkage.sh` was doing by hand. `awesome-nav`
additionally errors when a `.nav.yml` names a file that does not exist, covering
`check-orphans.sh`'s case from the other direction.

### 3.2 The `docs_dir` naming trap

MkDocs' `site_dir` defaults to `site/` relative to `mkdocs.yml`. Earlier drafts of the
reorganisation proposed `docs/wiki-default/` → **`docs/site/`**, which would make `docs_dir`
and `site_dir` collide and MkDocs will refuse to build.

**Use `docs/content/` as `docs_dir`.** The layout in
[reorganisation-plan.md](reorganisation-plan.md) §3 has been corrected accordingly.

decode-orc puts `mkdocs.yml` at its repo root with `docs_dir: docs`. DdD deviates
deliberately: `mkdocs.yml` lives in `docs/` so the docs component is self-contained
(`src = ./docs`) and does not have to pull in the whole repo. Build with
`mkdocs build -f docs/mkdocs.yml`. The theme, extensions and plugins — the part that was
actually asked for — are identical.

### 3.3 Navigation

`Sidebar.md` currently encodes a grouping that does **not** match the directory structure —
"General", "Ordering and building", "Capture Application" and "Support" all draw pages out of
a single `Misc/` folder, while "Pioneer LD-V4300D" draws from `Hardware/`. `awesome-nav`
derives navigation from the tree, so the content has to be reorganised to match the nav the
sidebar already describes:

```
content/
├── index.md
├── .nav.yml
├── general/            # Overview, User Guide, Laserdisc Player, Digital Media Preservation
├── ordering/           # How-To-Order, How-To-Flash-Firmware
├── capture-application/# User Guide (Linux), Windows Releases, MacOS Releases
├── related-projects/   # The ld-decode Family
├── development/        # Hardware Guide, Software Guide
├── ldv4300d/           # the seven Pioneer LD-V4300D pages
└── support/            # Submitting a bug report, Community, Donations
```

Root `.nav.yml`, transcribing the existing sidebar order:

```yaml
nav:
  - index.md
  - General: general
  - Ordering and building: ordering
  - Capture Application: capture-application
  - The Decode Family: related-projects
  - Development Guides: development
  - Pioneer LD-V4300D: ldv4300d
  - Support: support
```

with a per-directory `.nav.yml` fixing page order inside each section — otherwise `ldv4300d/`
sorts alphabetically and the LD-V4300D pages lose their deliberate
Overview → Cleaning → RF-Output → Calibration → PSU-Recap progression.

`Sidebar.md` is the authoritative source for all of this; transcribe it, then delete it.

Note `Misc/User-Guide.md` is currently linked **twice** from the sidebar ("User Guide" under
General, "User Guide for Linux" under Capture Application). MkDocs nav wants one home per
page — pick one (Capture Application reads more accurate) and cross-link from the other.

`Footer.md`'s CC BY-SA notice becomes Material's `copyright:` key.

### 3.4 URLs change — and that is why this belongs with the move

Jekyll produced `…/Related-Projects/The-ld-decode-Family.html`. MkDocs produces directory
URLs — `…/related-projects/the-ld-decode-family/`. Every inbound deep link breaks.

That is **already happening** because of D10: the site moves off
`simoninns.github.io/DomesdayDuplicator-docs` when docs folds into the monorepo. Doing the
theme conversion in the same phase means external links break **exactly once**, not twice.
This is the strongest argument for sequencing the two together, and it is why the theme
migration is not deferred to a later phase.

Known inbound links that must be updated in the same PR:

- `README.md:3` — the main documentation link
- `README.md:37` — a deep link to `Related-Projects/The-ld-decode-Family.html`
- `docs/README.md:7`
- `CONTRIBUTING.md:13`

## 4. Target `docs/mkdocs.yml`

Mirrors decode-orc's theme block; only the identity fields differ.

```yaml
site_name: Domesday Duplicator Documentation
site_url: https://simoninns.github.io/domesdayduplicator      # per P0-4
repo_url: https://github.com/simoninns/domesdayduplicator
repo_name: simoninns/domesdayduplicator
copyright: >
  All content provided under the Attribution-ShareAlike 4.0 International
  (CC BY-SA 4.0) license.

docs_dir: content            # NOT "site" — collides with the default site_dir

theme:
  name: material
  logo: assets/DdD-logo.svg          # from graphics/, see §6
  favicon: assets/favicon.ico
  palette:
    - scheme: default
      primary: indigo
      accent: indigo
      toggle:
        icon: material/brightness-7
        name: Switch to dark mode
    - scheme: slate
      primary: indigo
      accent: indigo
      toggle:
        icon: material/brightness-4
        name: Switch to light mode
  features:
    - navigation.sections
    - navigation.top
    - search.suggest
    - search.highlight
    - content.code.copy
    - toc.follow

extra_css:
  - assets/custom.css

markdown_extensions:
  - pymdownx.highlight:
      anchor_linenums: true
  - pymdownx.superfences
  - pymdownx.tasklist:
      custom_checkbox: true
  - admonition
  - pymdownx.details
  - tables
  - attr_list
  - md_in_html
  - toc:
      permalink: true

plugins:
  - search
  - awesome-nav
```

## 5. Target `docs/package.nix` and flake

Same shape as decode-orc's `decode-orc-docs` derivation, adapted to the per-component `package.nix`
pattern from [nix-flake-design.md](nix-flake-design.md) §1:

```nix
# docs/package.nix
{ lib, stdenvNoCC, python312 }:

let
  mkdocsEnv = python312.withPackages (ps: [
    ps.mkdocs
    ps.mkdocs-material
    ps."mkdocs-awesome-nav"
  ]);
in
stdenvNoCC.mkDerivation {
  pname = "domesday-duplicator-docs";
  version = "0";
  src = ./.;

  nativeBuildInputs = [ mkdocsEnv ];

  buildPhase = ''
    runHook preBuild
    mkdocs build --strict          # --strict: broken internal links fail the build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp -r site/* $out/
    runHook postInstall
  '';

  meta = {
    description = "Domesday Duplicator documentation site";
    license = lib.licenses.cc-by-sa-40;
    platforms = lib.platforms.all;
  };
}
```

`docs/shell.nix` exposes the same `mkdocsEnv` so `mkdocs serve` works for live preview —
that is the replacement for `build-local.sh`.

Note `--strict` in the packaged build but **not** in the dev shell, so a work-in-progress
link does not block local preview.

## 6. Assets and branding

- `graphics/` at the repo root holds the project logo and screenshots, referenced by the
  READMEs. Material wants `theme.logo` inside `docs_dir`. Either copy the logo into
  `content/assets/` or symlink at build time — **copy**, to keep the derivation's `src`
  closed over `./docs`.
- decode-orc uses an SVG logotype. DdD has PNGs in `graphics/`; an SVG is preferable for the
  header but a PNG works. Not a blocker.
- `content/assets/custom.css` starts empty (or with the small overrides needed to match DdD
  branding). Keeping the filename identical to decode-orc's makes the two sites easy to
  diff.
- The existing per-folder `assets/` directories move with their pages; relative image links
  in the markdown keep working, since MkDocs resolves them the same way Jekyll's
  `jekyll-relative-links` did.

## 7. CI

Replace `deploy-pages.yml` wholesale with decode-orc's version, changing only the paths:

```yaml
on:
  push:
    branches: [ master ]
    paths: [ 'docs/**', '.github/workflows/deploy-docs.yml' ]

jobs:
  build:
    steps:
      - uses: actions/checkout@v7
      - uses: cachix/install-nix-action@v31
      - run: nix build .#docs-site --print-build-logs
      - uses: actions/upload-pages-artifact@v5
        with:
          path: ./result
  deploy:
    needs: build
    steps:
      - uses: actions/deploy-pages@v5
```

This drops `actions/jekyll-build-pages` and, with it, the whole class of "works locally,
differs on Pages" problems — the artefact uploaded is exactly what `nix build` produced.

The three bespoke validation steps in the current workflow are absorbed:
`check-internal-linkage.sh` and `check-orphans.sh` by `--strict` plus `awesome-nav`; the
"no external links in the sidebar" check becomes moot once `Sidebar.md` is gone, since
`awesome-nav` builds navigation from the tree and cannot emit an external nav link. If that
policy still matters, re-add it as a grep over the `.nav.yml` files.

## 8. Task list (replaces Phase 4)

| Task | Size | Detail |
| --- | --- | --- |
| **P4-1** Reorganise content into nav-shaped directories | M | §3.3; `git mv` the 24 files; resolve the duplicated `User-Guide.md` nav entry |
| **P4-2** Author `mkdocs.yml` | S | §4 |
| **P4-3** Author `.nav.yml` files | S | Transcribe `Sidebar.md`, then delete it |
| **P4-4** Delete the Jekyll machinery | S | §3.1 — layouts, includes, `_config.yml`, `Sidebar.md`, `Footer.md`, `search.json`, and the four shell scripts. Resolves **D9** and **D12** |
| **P4-5** Delete `Unused-Assets/` | S | 6 MB that would otherwise ship in the site |
| **P4-6** `docs/package.nix`, `shell.nix` | M | §5 |
| **P4-7** Branding assets | S | §6 — logo into `content/assets/`, `custom.css`, favicon |
| **P4-8** Fix `site_url` and every inbound link | M | §3.4 + P0-4's decision. **D10** |
| **P4-9** Replace the Pages workflow | S | §7. Can land with P7 instead if preferred |
| **P4-10** Visual review | M | Every one of the 24 pages rendered under Material — tables, images and code blocks are where Kramdown-GFM and Python-Markdown differ most |

**Gate:** `nix build .#docs-site` succeeds with `--strict`; all 24 pages render with working
images; navigation matches the old sidebar's grouping and order; the deployed site and the
locally built one are the same derivation output.

## 9. Risks

| Risk | Severity | Mitigation |
| --- | --- | --- |
| Inbound deep links break | Medium | Unavoidable and already implied by the repo move — do both in one phase so it happens once. Consider a redirect stub at the old Pages URL (P0-4) |
| Kramdown → Python-Markdown rendering differences | Low | Only 24 files, no front matter, no Liquid. P4-10 is a page-by-page look, not a rewrite |
| `awesome-nav` behaviour differs from the hand-written sidebar | Low | `Sidebar.md` is transcribed directly into `.nav.yml`; diff the rendered nav against the current site before deleting it |
| Two projects' docs toolchains drift apart again | Low | Keep `mkdocs.yml`'s theme block byte-identical to decode-orc's except for identity fields, so a diff shows real divergence |
