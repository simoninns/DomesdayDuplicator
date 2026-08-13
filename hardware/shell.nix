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
# Dev shell only, no packaged export. The board file is on the current KiCad format, so
# `kicad-cli`-driven Gerber plotting works: see pcb/tools/plot-fab.sh. The schematics are
# still in the KiCad 4 format and have to be converted in the GUI before schematic PDF and
# BOM generation can be scripted; pcb/MIGRATION.md has the procedure.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-hardware";

  packages = [ pkgs.kicad ];

  shellHook = ''
    echo "Domesday Duplicator — hardware development shell"
    echo
    echo "  kicad 'pcb/Domesday Duplicator.pro'"
    echo
    echo "Note: the board is on the current KiCad format but the schematics are still"
    echo "KiCad 4. Opening the project offers to convert them — that is a large diff and"
    echo "should be a deliberate commit, not a side effect of opening the project."
    echo "See pcb/MIGRATION.md before accepting."
    echo
  '';
}
