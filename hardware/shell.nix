# Development shell for the PCB design.
#
#   nix develop ./hardware   (or `nix develop .#hardware` from the repository root)
#   kicad hardware/pcb/"Domesday Duplicator.pro"
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
