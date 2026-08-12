{
  description = "Domesday Duplicator — LaserDisc RF capture hardware, gateware, firmware and software";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (import ./nix/lib.nix { inherit nixpkgs; }) forAllSystems;
    in
    {
      # Components are callPackage'd from the same .nix files their own flakes use, so there
      # is exactly one definition of each and no cross-flake inputs to keep in step.
      #
      # Not here, deliberately:
      #   fx3-firmware  — Phase 5, needs the ARM cross build packaged
      #   bitstream     — Phase 6, and never aggregated: Quartus is unfree, x86_64-linux
      #                   only and redistributable = false, so it can never come from a
      #                   binary cache. It stays behind fpga/flake.nix and, by the same
      #                   reasoning, out of CI — the bitstream is built locally and attached
      #                   to releases by hand. See docs-tech/implementation-plan.md,
      #                   "Release artefacts and provenance".
      # Merged per system, not with `//` across two forAllSystems/forLinux calls: `//` is
      # a shallow update, so the Linux set would replace the portable one wholesale and
      # `gui` would silently disappear on exactly the systems that can build it.
      packages = forAllSystems (
        pkgs:
        rec {
          gui = pkgs.qt6Packages.callPackage ./gui/package.nix { };
          docs-site = pkgs.callPackage ./docs/package.nix { };
          default = gui;
        }
        // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          fx3-programmer = pkgs.callPackage ./fx3/programmer/package.nix { };
        }
      );

      devShells = forAllSystems (pkgs: {
        default = import ./nix/shell.nix { inherit pkgs; };
        gui = import ./gui/shell.nix { inherit pkgs; };
        fx3 = import ./fx3/shell.nix { inherit pkgs; };
        fpga = import ./fpga/shell.nix { inherit pkgs; };
        hardware = import ./hardware/shell.nix { inherit pkgs; };
        docs = import ./docs/shell.nix { inherit pkgs; };
      });

      # Every package doubles as a check: each one runs its own ctest suite during
      # buildPhase, so `nix flake check` builds and tests the whole tree.
      checks = forAllSystems (pkgs: self.packages.${pkgs.stdenv.hostPlatform.system});

      nixosModules = {
        udev = ./nix/modules/udev.nix;
        default = self.nixosModules.udev;
      };

      # Lets a NixOS configuration get the packages under their conventional attribute names,
      # which is what hardware.domesdayDuplicator.package defaults to.
      overlays.default =
        final: _prev:
        {
          domesday-duplicator-gui = final.qt6Packages.callPackage ./gui/package.nix { };
        }
        // final.lib.optionalAttrs final.stdenv.hostPlatform.isLinux {
          domesday-duplicator-fx3-programmer = final.callPackage ./fx3/programmer/package.nix { };
        };

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
