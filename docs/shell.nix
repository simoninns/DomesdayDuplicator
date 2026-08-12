# Development shell for the documentation site.
#
#   nix develop ./docs       (or `nix develop .#docs` from the repository root)
#   mkdocs serve             live preview on http://127.0.0.1:8000
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
