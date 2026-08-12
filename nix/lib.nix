# Shared helpers for the repository's flakes.
#
# Every component flake and the root flake go through here, so the set of supported systems
# and the nixpkgs configuration are defined exactly once.
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
  #   - fpga                        → x86_64-linux only; Quartus is unfree and Linux-x86 only,
  #                                   so fpga/ carries its own flake and is not aggregated here
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

  # No config.allowUnfree here: everything reachable from the root flake is free software.
  # fpga/flake.nix sets allowUnfree on its own nixpkgs import, so that the unfree dependency
  # stays contained in the one component that actually needs it and consumers of the root
  # flake never need --impure.
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
