# The Domesday Duplicator documentation site.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The site the GitHub Pages workflow deploys is this derivation's output, byte for byte —
# the workflow builds it with Nix and uploads ./result. That removes the whole class of
# "renders differently on Pages" problems the previous Jekyll route was exposed to.

{
  lib,
  stdenvNoCC,
  python312,
}:

let
  mkdocsEnv = python312.withPackages (ps: [
    ps.mkdocs
    ps.mkdocs-material
    ps.mkdocs-awesome-nav
  ]);
in
stdenvNoCC.mkDerivation {
  pname = "domesday-duplicator-docs";
  version = "0";

  # ./docs only. mkdocs.yml lives here rather than at the repository root precisely so this
  # stays scoped to the documentation component.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./mkdocs.yml
      ./content
    ];
  };

  nativeBuildInputs = [ mkdocsEnv ];

  buildPhase = ''
    runHook preBuild

    # --strict promotes MkDocs' warnings to errors, so a broken internal link, a nav entry
    # pointing at a missing file, or an orphaned page fails the build. This is what replaces
    # the hand-written check-internal-linkage.sh and check-orphans.sh.
    mkdocs build --strict

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out
    cp -r site/. $out/

    # A site without an entry point is a broken deploy that looks like a successful build
    if [ ! -f "$out/index.html" ]; then
      echo "mkdocs produced no index.html — refusing to install an empty site" >&2
      exit 1
    fi

    runHook postInstall
  '';

  meta = {
    description = "Domesday Duplicator documentation site";
    homepage = "https://simoninns.github.io/domesdayduplicator";
    license = lib.licenses.cc-by-sa-40;
    platforms = lib.platforms.all;
  };
}
