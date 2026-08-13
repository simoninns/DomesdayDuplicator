# The only flake in the repository, and so the only flake.lock.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Components carry package.nix and shell.nix, never a flake.nix of their own. An earlier
# layout gave each component a thin flake so that `cd gui && nix develop` worked; every one
# of those resolved `nixos-unstable` into its own lock file, so entering the tree through a
# component quietly got a different nixpkgs from the pin here. Reproducibility is worth more
# than the shorthand, and nothing is lost: Nix walks up to find this file, so
# `nix develop .#gui` works from any subdirectory.
{
  description = "Domesday Duplicator — LaserDisc RF capture hardware, gateware, firmware and software";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (import ./nix/lib.nix { inherit nixpkgs; }) forAllSystems;

      # The commit the working tree is at, for artefacts that carry their own provenance:
      # the FX3 firmware's USB product descriptor (D4) and the GUI's About dialog and
      # --version (D21). A build from a tag or a tarball has no .git for CMake's fallback
      # to consult, so passing it explicitly is what stops a release artefact silently
      # reporting "unknown" — which P7-9 makes a release gate.
      version = self.shortRev or self.dirtyShortRev or "unknown";

      # Quartus is unfree, so it needs its own `pkgs` — but not its own flake, which would
      # bring back the second lock file the single-flake layout exists to prevent. This is a
      # second import of the *same locked* nixpkgs with allowUnfree set: consumers need
      # neither --impure nor NIXPKGS_ALLOW_UNFREE, evaluation stays pure, and the unfree
      # dependency cannot reach any free output because nothing else is built from this set.
      #
      # x86_64-linux only, because the nixpkgs package is
      # (platforms = [ "x86_64-linux" ], redistributable = false).
      quartusSystem = "x86_64-linux";
      unfreePkgs = import nixpkgs {
        system = quartusSystem;
        config.allowUnfree = true;
      };

      # supportedDevices defaults to six device families and each one is a separate
      # multi-hundred-megabyte download. The board is an EP4CE22F17C6, so one family
      # covers it, and dropping the other five is the largest size saving available.
      # withQuesta: this project simulates with iverilog and verilator, both free.
      quartus = unfreePkgs.quartus-prime-lite.override {
        supportedDevices = [ "Cyclone IV" ];
        withQuesta = false;
      };
    in
    {
      # Components are callPackage'd from the same .nix files their own flakes use, so there
      # is exactly one definition of each and no cross-flake inputs to keep in step.
      #
      # `bitstream` is the exception to every rule here: it is the only unfree output, it
      # exists on x86_64-linux alone, and it is deliberately absent from `checks` — Quartus
      # is redistributable = false, so it can never come from a binary cache, and the
      # bitstream is built locally and attached to releases by hand rather than by CI. The
      # decision and the three ways it could reach CI later are in fpga/README.md,
      # "Why this is not built by CI".
      #
      # Merged per system, not with `//` across two forAllSystems/forLinux calls: `//` is
      # a shallow update, so the Linux set would replace the portable one wholesale and
      # `gui` would silently disappear on exactly the systems that can build it.
      packages = forAllSystems (
        pkgs:
        rec {
          gui = pkgs.qt6Packages.callPackage ./gui/package.nix { dddVersion = version; };
          docs-site = pkgs.callPackage ./docs/package.nix { };

          # fx3-mkimage is exposed rather than hidden inside the firmware derivation
          # because a contributor building the firmware outside Nix needs it on PATH too —
          # without it CMakeLists.txt falls back to compiling it from source each time.
          fx3-mkimage = pkgs.callPackage ./fx3/mkimage/package.nix { dddVersion = version; };
          fx3-firmware = pkgs.callPackage ./fx3/firmware/package.nix {
            inherit fx3-mkimage;
            firmwareVersion = version;
          };

          default = gui;
        }
        // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          fx3-programmer = pkgs.callPackage ./fx3/programmer/package.nix { };
        }
        // pkgs.lib.optionalAttrs (pkgs.stdenv.hostPlatform.system == quartusSystem) {
          bitstream = unfreePkgs.callPackage ./fpga/package.nix {
            quartus-prime-lite = quartus;
            bitstreamVersion = version;
          };
        }
      );

      devShells = forAllSystems (
        pkgs:
        {
          default = import ./nix/shell.nix { inherit pkgs; };
          gui = import ./gui/shell.nix { inherit pkgs; };
          fx3 = import ./fx3/shell.nix { inherit pkgs; };
          # Free tools only — lint and simulate the Verilog with no Quartus download.
          fpga = import ./fpga/shell.nix { inherit pkgs; };
          hardware = import ./hardware/shell.nix { inherit pkgs; };
          docs = import ./docs/shell.nix { inherit pkgs; };
        }
        // pkgs.lib.optionalAttrs (pkgs.stdenv.hostPlatform.system == quartusSystem) {
          # The full toolchain: everything in `fpga` plus Quartus itself. This shell,
          # rather than `nix build .#bitstream`, is the deliverable of Phase 6 — the
          # bitstream build is what makes it repeatable, but the shell is where the work
          # is done.
          fpga-quartus = import ./fpga/quartus-shell.nix {
            pkgs = unfreePkgs;
            inherit quartus;
          };
        }
      );

      # Every package doubles as a check: each one runs its own ctest suite during
      # buildPhase, so `nix flake check` builds and tests the whole tree. The gateware has
      # no ctest suite and no package that CI can build, so its lint and simulation checks
      # are added here explicitly — they are the only automated coverage the Verilog gets.
      # The licence-header check belongs to no component at all, so it comes from nix/.
      #
      # `bitstream` is removed rather than never added, so that a future package added to
      # the unfree set cannot reach `checks` by being forgotten about here.
      checks = forAllSystems (
        pkgs:
        let
          fpgaChecks = pkgs.callPackage ./fpga/checks.nix { };
          repoChecks = pkgs.callPackage ./nix/checks.nix { src = self; };
        in
        builtins.removeAttrs self.packages.${pkgs.stdenv.hostPlatform.system} [ "bitstream" ]
        // {
          fpga-lint = fpgaChecks.lint;
          fpga-sim = fpgaChecks.sim;
          fpga-provenance = fpgaChecks.provenance;
          licence-headers = repoChecks.licence-headers;
        }
      );

      nixosModules = {
        udev = ./nix/modules/udev.nix;
        default = self.nixosModules.udev;
      };

      # Lets a NixOS configuration get the packages under their conventional attribute names,
      # which is what hardware.domesdayDuplicator.package defaults to.
      overlays.default =
        final: _prev:
        {
          domesday-duplicator-gui = final.qt6Packages.callPackage ./gui/package.nix {
            dddVersion = version;
          };
        }
        // final.lib.optionalAttrs final.stdenv.hostPlatform.isLinux {
          domesday-duplicator-fx3-programmer = final.callPackage ./fx3/programmer/package.nix { };
        };

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
