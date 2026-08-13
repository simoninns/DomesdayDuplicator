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
# Dev shell only, no packaged export. The whole project is on the current KiCad format, so
# `kicad-cli`-driven Gerber, drill, schematic PDF and BOM generation all work: see
# pcb/tools/plot-fab.sh.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-hardware";

  packages = [ pkgs.kicad ];

  shellHook = ''
    echo "Domesday Duplicator — hardware development shell"
    echo
    echo "  kicad 'pcb/Domesday Duplicator.kicad_pro'"
    echo
    echo "Plot the fabrication outputs for a revision with:"
    echo
    echo "  pcb/tools/plot-fab.sh <REV>"
    echo
    echo "It will not overwrite a revision that already exists. See pcb/MIGRATION.md for"
    echo "what is still outstanding after the KiCad 10 conversion."
    echo
  '';
}
