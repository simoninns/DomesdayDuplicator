# Folding the submodules back into the monorepo

Companion to [reorganisation-plan.md](reorganisation-plan.md) §6, phases 0–2.

**Do this on a scratch clone first.** Every command below rewrites history in some way, and
step 4 is a force-push.

## 1. What we are merging

| Submodule | Commits | History size | Target path (phase 1) | Final path (phase 2) |
| --- | --- | --- | --- | --- |
| `docs` | 19 | 140 MB | `docs/` | `docs/content/` + `docs/mkdocs.yml` |
| `firmware` | 115 | 26 MB | `firmware/` | `fpga/` + `fx3/` |
| `gui-app` | 299 | 2.8 MB | `gui-app/` | `gui/` |
| `hardware` | 40 | 5.6 MB | `hardware/` | `hardware/pcb/` + `hardware/doc/` |

Keep phase 1 (history merge, paths unchanged) separate from phase 2 (renames). Doing both at
once makes the merge commit impossible to review and defeats `git log --follow`.

## 2. Approach

Two options. **Use option A.**

**Option A — `git filter-repo` + unrelated-history merge.** Rewrite each submodule's history
so every commit's paths are prefixed with the target directory, then merge it into the
superproject as an unrelated history. Result: real, walkable, per-file history, no
`git log` gymnastics. Requires `git-filter-repo` (`nix shell nixpkgs#git-filter-repo`).

**Option B — `git subtree add --prefix=... <url> <branch>`.** One command, no rewriting, but
history lands behind a synthetic subtree merge commit and `git log -- path` is less
straightforward. Acceptable if `filter-repo` is unavailable; not recommended.

## 3. Procedure (option A)

### 3.0 Prep — phase 0

> ## Executed 2026-08-12 — this document is now a record, not a plan
>
> The merge was performed on local branch `monorepo-merge` and **has not been pushed**. What
> was actually done differs from the procedure drafted below in two ways, both following
> P0-1's revision:
>
> 1. **Sources were the local `.git/modules/<m>` object stores, not GitHub clones** — which
>    guarantees the exact pinned commits and needs no network.
> 2. **Each source was reduced to a single ref at the pinned SHA** before rewriting, so no
>    other branch, no tag and no post-pin commit could enter the monorepo.
>
> Consequently the prep checklist in §3.0 was **not** needed: nothing was pushed to the
> submodules, no pins were moved, and no `pre-monorepo` tags were created. It is retained
> below for the record, and steps 1–2 of it remain relevant if the `fpgaupdate-202512` work
> is ever brought over.
>
> The five vendored Cypress files were copied out before `git rm -f` and restored afterwards;
> they are untracked in the merged tree, awaiting their own commit.

### State as of 2026-08-12

```
docs      default=main    pinned=9e57a729  tip=941e0def   2 commits BEHIND
firmware  default=master  pinned=83a98bbc  tip=83a98bbc   same (but HEAD is detached)
gui-app   default=master  pinned=8036eaf1  tip=ac980a09   1 commit BEHIND
hardware  default=master  pinned=76099311  tip=76099311   same
```

`docs` and `gui-app` pin commits *behind* their remote tips — in both cases a merged pull
request. Since the import takes the **tip**, the monorepo will contain those commits even
though today's checkout does not. Update the pins first so what you test before the merge is
what you get after it.

### Prep checklist — in this order

**Order matters.** Step 1 is a fast-forward, and it only stays a fast-forward while
`firmware`'s `master` has no commits of its own. Committing the vendored SDK files (step 2)
first would add one, `--ff-only` would then fail, and you would need a real merge.

```bash
cd /path/to/domesdayduplicator

# 1. Land the FPGA work (P0-1). MUST come before step 2.
git -C firmware checkout master                      # currently detached
git -C firmware merge --ff-only origin/fpgaupdate-202512
git -C firmware push origin master

# 2. Commit the vendored Cypress files added during Phase 0
git -C firmware add fx3/fx3-firmware/cyfx3sdk/README.md \
                    fx3/fx3-programmer/cyfxflashprog.img \
                    fx3/fx3-programmer/cyfxflashprog.txt \
                    fx3/fx3-programmer/LICENSE.cyusb_linux.txt \
                    fx3/fx3-programmer/VENDOR.md
git -C firmware commit -m "Vendor the FX3 flash programmer image and record SDK provenance"
git -C firmware push origin master

# 3. Bring the two stale submodules onto their tips
git -C docs    checkout main   && git -C docs    merge --ff-only origin/main
git -C gui-app checkout master && git -C gui-app merge --ff-only origin/master
git -C hardware checkout master                      # already at tip; just leave detached HEAD

# 4. Tag each repo's pre-merge state (cheap insurance; not imported)
for m in docs firmware gui-app hardware; do
  git -C "$m" tag -a pre-monorepo -m "State before folding into the DomesdayDuplicator monorepo"
  git -C "$m" push origin pre-monorepo
done

# 5. Record the new pointers in the superproject
git add docs firmware gui-app hardware
git commit -m "Update sub-module pointers ahead of the monorepo merge"
```

After step 5, `git submodule status` should show all four at their default-branch tips with
no local modifications, and `git -C <m> status` should be clean for each. Only then start
§3.1.

The superproject's `origin/release-2.x` is left untouched: 2022-era, on the pre-split flat
layout, and its content is already in `master`.

### 3.1 Rewrite each submodule into a subdirectory

Work on throwaway clones:

**Only the default branch of each repository is imported** (P0-1 amendment) — no other
branches, no tags. `--single-branch --no-tags` does this at clone time, which is cleaner than
filtering refs afterwards.

Note the default branch name is **not uniform**: `docs` uses `main`, the other three use
`master`. "Only main" means "only each repository's default branch", which is what the
`--branch` arguments below name.

```bash
WORK=$(mktemp -d)

clone_and_prefix() {
  local repo="$1" prefix="$2" branch="$3"
  git clone --single-branch --no-tags --branch "$branch" \
    "https://github.com/simoninns/DomesdayDuplicator-${repo}.git" "$WORK/$repo"
  git -C "$WORK/$repo" filter-repo --to-subdirectory-filter "$prefix"
}

clone_and_prefix docs     docs     main      # note: main, not master
clone_and_prefix firmware firmware master    # must already contain the landed FPGA work
clone_and_prefix gui-app  gui-app  master
clone_and_prefix hardware hardware master
```

`--to-subdirectory-filter` moves every path in every commit under the prefix, so the merge
in 3.2 cannot conflict with the superproject's tree.

Anything not on a default branch stays behind in the old repository, which P0-6 leaves in
place — nothing is destroyed, but nothing else is carried across either. This is why P0-1's
fast-forward of `firmware`'s `master` must happen **first**: otherwise the Quartus 25.1 work
is simply not imported.

Note the submodules carry their own `.github/workflows/`, `LICENSE`, `README.md`,
`.gitignore` and `CONTRIBUTING.md`. After the prefix rewrite these land at e.g.
`firmware/.github/workflows/…` — inert (GitHub only reads the repo-root `.github/`), which is
the desired outcome for phase 1. They get consolidated in phase 2.

### 3.2 Remove the submodule bindings, then merge

```bash
cd /path/to/domesdayduplicator
git checkout -b monorepo-merge master

# Deregister the submodules and free the paths, but keep .git/modules until the merge
# is verified — it is the local backup of the pinned commits.
for m in docs firmware gui-app hardware; do
  git submodule deinit -f "$m"
  git rm -f "$m"
done
git rm -f .gitmodules
git commit -m "Remove submodule bindings ahead of monorepo merge"

# Merge each rewritten history in
for m in docs firmware gui-app hardware; do
  git remote add "import-$m" "$WORK/$m"
  git fetch "import-$m"
  git merge --allow-unrelated-histories --no-ff \
    -m "Merge $m repository into the monorepo (history preserved)" \
    "import-$m/$(git -C "$WORK/$m" symbolic-ref --short HEAD)"
  git remote remove "import-$m"
done
```

### 3.3 Verify before pushing

```bash
# Tree matches the pre-merge submodule checkouts (expect no output)
for m in docs firmware gui-app hardware; do
  diff -r --exclude=.git "$m" "/path/to/old-checkout/$m" && echo "$m OK"
done

# Per-file history survived
git log --oneline --follow -- gui-app/tools/DomesdayDuplicator/mainwindow.cpp | tail -5
git log --oneline --follow -- firmware/DE0-NANO/DomesdayDuplicator/DomesdayDuplicator.v | tail -5

# Commit count is roughly 458 + 19 + 115 + 299 + 40 + 5 merge/removal commits
git rev-list --count HEAD

# No submodule remnants
git ls-files -s | awk '$1 == "160000"'   # expect empty
test ! -f .gitmodules && echo "no .gitmodules OK"

# Clean clone works with no SSH key and no --recursive
git clone --no-local . /tmp/ddd-clone-test && ls /tmp/ddd-clone-test
```

That last check is the point of the whole exercise: three of the four submodule URLs are
`git@github.com:` SSH, so the `git clone --recursive` command in the current README fails for
anyone without a GitHub key.

### 3.4 Land it

```bash
git push origin monorepo-merge
# open a PR, then merge with a MERGE COMMIT — not squash, not rebase.
```

Squashing or rebasing this PR destroys the imported histories and silently defeats the entire
migration. Set the branch protection accordingly, or merge it locally.

## 4. After the merge

1. **Do nothing to the four upstream repos.** Per [decisions.md](decisions.md) P0-6 they are
   left alone — neither archived nor deleted — and the maintainer will clean them up
   separately. Note the consequence: they stay writable and their READMEs will not mention
   the monorepo, so item 2 below carries the whole burden of telling people where to work.
2. **Update the superproject README**: delete the `git clone --recursive` instructions and
   the submodule list, replace with a plain clone, and state plainly that this repository is
   now the only place to work.
3. **Update `CONTRIBUTING.md`**: it currently routes documentation changes to the separate
   docs repo, which still exists and is still writable (P0-6) — so this redirect matters more
   than it would if that repo had been archived.
4. **Retire the per-submodule CI**: after phase 2, delete the `*/.github/` directories and
   consolidate into a root workflow.
5. **Keep `.git/modules` locally** until the pushed monorepo has been verified by someone
   other than you.

## 5. Shrinking the result — decided: do nothing

**[decisions.md](decisions.md) P0-5 chose option 1 below: accept ~400 MB.** `filter-repo` is
used for path prefixing only — no `--strip-blobs-bigger-than`, no LFS. The options are kept
here because P0-2 could still force a rewrite (see the caveat at the end of this section).

The merged repo will be roughly 400 MB of git data. Two candidates dominate:

- `firmware`'s vendored CyFX3 SDK: four ~6–9 MB `.a` files × several revisions. Only the
  `fx3_release` profile is referenced by the build; `fx3_debug`, `fx3_profile_debug` and
  `fx3_profile_release` are dead weight (~45 MB of the 71 MB checkout).
- `docs`' image assets: 140 MB of history for 19 commits, e.g. a single 8.3 MB fabrication
  photo.

Options, in increasing order of disruption:

1. **Do nothing.** 400 MB is unpleasant but survivable, and everyone cloning `--recursive`
   already pays it today. **This is the default recommendation.**
2. **Delete the unused SDK profiles going forward.** Shrinks the checkout by ~45 MB, leaves
   history untouched, entirely reversible. Worth doing regardless.
3. **Strip large blobs from history during step 3.1**, e.g.
   `git filter-repo --strip-blobs-bigger-than 5M` on the `docs` clone. This changes what
   contributors can check out at old revisions and invalidates every existing SHA for that
   submodule. Irreversible.
4. **Git LFS** for `docs/content/**/assets/**` and the SDK archives. Adds a hard dependency for
   every contributor and does not help GitHub Pages, which needs the real files.

If option 3 or 4 is wanted, it must happen **during** step 3.1 — retrofitting it after the
merge means a second history rewrite and a second force-push.

**Previously-open caveat, now closed.** P0-2 decided to vendor the Cypress SDK regardless of
the licence review, so no blob removal is required and this section stays at option 1. The
merge can proceed without waiting on any licence question.

## 6. Rollback

Before step 3.4, rollback is free: delete the branch. After the push, `master` is unchanged
until the PR merges, so rollback is closing the PR. After the merge, the four `pre-monorepo`
tags plus the pre-merge `master` SHA are enough to reconstruct the old arrangement — record
that SHA in the merge commit message.
