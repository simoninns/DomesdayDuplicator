# Shared helpers for the repository's flake.
#
# There is exactly one flake.nix, at the repository root, and so exactly one flake.lock.
# Components used to carry thin flakes of their own for `cd gui && nix develop`, but each
# one grew its own lock file resolving `nixos-unstable` independently, which meant entering
# through a component silently got a different nixpkgs from the root's pin. A single lock is
# worth more than the shorthand. Everything goes through here, so the set of supported
# systems and the nixpkgs configuration are defined exactly once.
#
# Usage:
#   inherit (import ./nix/lib.nix { inherit nixpkgs; }) forAllSystems forLinux;
#   packages = forAllSystems (pkgs: { gui = pkgs.callPackage ./gui/package.nix { }; });
#
# The callback receives a `pkgs` rather than a system string, because every use site wants
# `pkgs` and threading the string through adds a `let` block to each one.

{ nixpkgs }:

let
  # Components differ in reach:
  #   - gui, fx3/programmer, docs   → portable
  #   - fx3/firmware                → cross-compiled from Linux only (see forLinux)
  #   - fpga (free tools)           → portable: lint and simulation need no Quartus
  #   - fpga (bitstream)            → x86_64-linux only; Quartus is unfree and Linux-x86 only
  # x86_64-darwin is deliberately absent: nixpkgs 26.11 dropped support for it, and
  # evaluating any attribute for that system now throws. Intel Macs are covered by the
  # Homebrew-based macOS jobs in CI, which remain the authoritative macOS coverage (P0-7).
  allSystems = [
    "x86_64-linux"
    "aarch64-linux"
    "aarch64-darwin"
  ];

  linuxSystems = [
    "x86_64-linux"
    "aarch64-linux"
  ];

  # No config.allowUnfree here: everything reachable through these helpers is free software,
  # so consumers never need --impure and nothing unfree can be pulled in by accident.
  #
  # Phase 6's Quartus shell needs allowUnfree, and gets it from a second import of *this same
  # locked* nixpkgs (`import nixpkgs { inherit system; config.allowUnfree = true; }`) rather
  # than from a flake of its own. Same pin, contained blast radius, still one flake.lock.
  pkgsFor = system: import nixpkgs { inherit system; };

  eachSystem = systems: f: nixpkgs.lib.genAttrs systems (system: f (pkgsFor system));
in
{
  inherit allSystems linuxSystems pkgsFor;

  # Apply f to every supported system.
  forAllSystems = eachSystem allSystems;

  # Apply f to the Linux systems only.
  forLinux = eachSystem linuxSystems;

  # The nixpkgs revision behind all of the above, for `nix flake metadata`-style reporting.
  inherit (nixpkgs) lib;
}
