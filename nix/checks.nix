# Repository-wide checks — the ones that belong to no single component.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every other check in this repository is scoped to a component and lives beside it
# (fpga/checks.nix, or the ctest suite inside a package's buildPhase). This file is for
# checks whose subject is the whole tree, so no component can own them.
#
# `src` is the flake source, so the check sees every tracked file and only tracked files —
# which is exactly the set the convention applies to. It also means any change anywhere
# re-runs the check. That is the honest cost of a whole-tree check, and it is cheap: the
# thing being run is a shell script over a few hundred headers.

{
  runCommand,
  bash,
  src,
}:

{
  # T4 — every project-authored source file carries a copyright and a licence statement.
  # Runs the same script a developer runs, so the two cannot drift.
  licence-headers = runCommand "ddd-licence-headers" { nativeBuildInputs = [ bash ]; } ''
    bash ${src}/tools/check-licence-headers.sh ${src}
    touch $out
  '';
}
