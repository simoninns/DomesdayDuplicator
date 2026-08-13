# Development shell for the PCB design.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#hardware
#   kicad hardware/pcb/"Domesday Duplicator.pro"
#
# There is a single flake.nix at the repository root and a single flake.lock beside it.
# Nix walks up to find them, so `.#hardware` resolves identically from `hardware/` and from
# the root. A bare `nix develop` gives the all-components default shell, not this one,
# whatever directory you happen to be in.
#
# Dev shell only, no packaged export. `kicad-cli`-driven Gerber/PDF/BOM generation is
# blocked on migrating the KiCad 5 project files to the current format — kicad-cli cannot
# read legacy .sch schematics. That migration is a separate change with its own review.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-hardware";

  packages = [ pkgs.kicad ];

  shellHook = ''
    echo "Domesday Duplicator — hardware development shell"
    echo
    echo "  kicad 'pcb/Domesday Duplicator.pro'"
    echo
    echo "Note: these are KiCad 5 files. Opening them in a current KiCad offers an in-place"
    echo "format upgrade — that is a large diff and should be a deliberate commit, not a"
    echo "side effect of opening the project."
    echo
  '';
}
