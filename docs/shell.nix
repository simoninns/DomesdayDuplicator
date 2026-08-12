# Development shell for the documentation site.
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#docs
#   mkdocs serve             live preview on http://127.0.0.1:8000
#
# There is a single flake.nix at the repository root and a single flake.lock beside it.
# Nix walks up to find them, so `.#docs` resolves identically from `docs/` and from the
# root. A bare `nix develop` gives the all-components default shell, not this one, whatever
# directory you happen to be in.
#
# This is the replacement for the old build-local.sh, which rebuilt the whole Jekyll site
# into a temporary directory on every edit.
#
# Note there is no --strict here, unlike package.nix: a work-in-progress link should not
# stop you previewing the page you are writing. The packaged build is where strictness
# belongs, because that is what ships.

{ pkgs }:

let
  mkdocsEnv = pkgs.python312.withPackages (ps: [
    ps.mkdocs
    ps.mkdocs-material
    ps.mkdocs-awesome-nav
  ]);
in
pkgs.mkShell {
  name = "ddd-docs";

  packages = [
    mkdocsEnv
    pkgs.yaml-language-server # mkdocs.yml and the .nav.yml files
  ];

  shellHook = ''
    echo "Domesday Duplicator — documentation shell"
    echo
    echo "  mkdocs serve -f docs/mkdocs.yml     live preview on http://127.0.0.1:8000"
    echo "  mkdocs build -f docs/mkdocs.yml --strict"
    echo
    echo "Navigation comes from the directory tree; per-directory .nav.yml files set the"
    echo "order. Adding a page with no .nav.yml entry appends it alphabetically."
    echo
  '';
}
