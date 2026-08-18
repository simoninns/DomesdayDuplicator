# Domesday Duplicator Documentation

Source for the project's documentation website, published at
**<https://simoninns.github.io/DomesdayDuplicator>**.

Built with [MkDocs](https://www.mkdocs.org/) and
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/), matching the toolchain
used by [decode-orc](https://github.com/decode-orc/decode-orc) — the two projects share a
lot of content, so they share a documentation stack.

## Contents

| Path | Contents |
| --- | --- |
| [content/](content/) | The site's markdown, one directory per navigation section |
| [mkdocs.yml](mkdocs.yml) | Site configuration: theme, extensions, plugins |
| `content/**/.nav.yml` | Navigation order within each section |
| `content/assets/` | Site-wide assets — logo, favicon, `custom.css` |

Each section directory keeps its own `assets/` beside the pages that use it, so image links
stay relative and short.

## Editing

Live preview, with rebuild-on-save. The `nix` commands work from anywhere in the working
tree — there is one `flake.nix`, at the repository root, and Nix walks up to find it — but
the `mkdocs` invocation below assumes you are **at the repository root**:

```bash
nix develop .#docs
mkdocs serve -f docs/mkdocs.yml     # http://127.0.0.1:8000
```

From inside `docs/` it is `mkdocs serve` with no `-f`. Either way, `nix develop .#docs` is
the same command in both places; only the `mkdocs` path changes.

Or build the site exactly as it is published:

```bash
nix build .#docs-site
```

## Adding a page

1. Put the markdown in the section directory it belongs to.
2. Add it to that directory's `.nav.yml`, in the position you want. A page with no entry is
   appended alphabetically, which is usually not what you want.
3. Put any images in that section's `assets/` directory and link them relatively.

**Use markdown image syntax, not raw `<img>` tags.** MkDocs rewrites relative paths inside
`![](…)` but passes raw HTML through untouched, and because pages are served from directory
URLs a raw `src="assets/…"` resolves one level too shallow and silently 404s. For sizing,
`attr_list` is enabled:

```markdown
![](assets/example.png){ width="600" }
```

## Checks

The packaged build runs `mkdocs build --strict`, so the build **fails** on a broken internal
link, a `.nav.yml` entry naming a missing file, or an orphaned page. That replaces the
hand-written link and orphan shell scripts the Jekyll site used.

`--strict` is deliberately not used by `mkdocs serve`, so a work-in-progress link does not
stop you previewing the page you are writing.

## Licensing

Site content is provided under the
[Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)](https://creativecommons.org/licenses/by-sa/4.0/)
licence, as stated in the footer of every page.

Note the `LICENSE` file in this directory is a GPLv3 copy inherited from when the
documentation lived in its own repository, and does not match that statement. The
repository's software licence is the root [LICENSE](../LICENSE).
