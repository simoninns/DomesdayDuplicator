# The only flake in the repository, and so the only flake.lock.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Components carry package.nix and shell.nix, never a flake.nix of their own. An earlier
# layout gave each component a thin flake so that `cd ddd-gui && nix develop` worked; every one
# of those resolved `nixos-unstable` into its own lock file, so entering the tree through a
# component quietly got a different nixpkgs from the pin here. Reproducibility is worth more
# than the shorthand, and nothing is lost: Nix walks up to find this file, so
# `nix develop .#ddd-gui` works from any subdirectory.
{
  description = "Domesday Duplicator — LaserDisc RF capture hardware, gateware, firmware and software";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (import ./nix/lib.nix { inherit nixpkgs; }) forAllSystems;

      # The commit the working tree is at, for artefacts that carry their own provenance:
      # the FX3 firmware's USB product descriptor, and the GUI's About dialog and
      # --version. A build from a tag or a tarball has no .git for CMake's fallback
      # to consult, so passing it explicitly is what stops a release artefact silently
      # reporting "unknown" — which the release workflows gate on.
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
      # `bitstream` and `quartus-prime-lite` are the exceptions to every rule here: they are
      # the only unfree outputs, they exist on x86_64-linux alone, and both are deliberately
      # absent from `checks`. Quartus is redistributable = false, so it can never come from
      # cache.nixos.org — which is why it stays out of the per-commit `nix flake check` tier
      # that every contributor runs. It is built by the dedicated bitstream and release
      # workflows instead; the full model is in the "Release pipeline" page of the
      # documentation site and in fpga/README.md, "How the bitstream is built".
      #
      # Merged per system, not with `//` across two forAllSystems/forLinux calls: `//` is
      # a shallow update, so the Linux set would replace the portable one wholesale and
      # `ddd-gui` would silently disappear on exactly the systems that can build it.
      packages = forAllSystems (
        pkgs:
        rec {
          # The capture application, and what every packaging and release workflow
          # builds.
          ddd-gui = pkgs.qt6Packages.callPackage ./ddd-gui/package.nix {
            dddVersion = version;
            # Present from the commit that publishes a release key and absent before it,
            # so this is the one place the flake asks whether the file exists rather than
            # asserting that it does. The alternative — a path that must exist — would
            # make every build of the tree fail until the key was generated.
            releaseUpdateKeyFile =
              if builtins.pathExists ./tools/keys/release.pub then ./tools/keys/release.pub else null;
          };

          docs-site = pkgs.callPackage ./docs/package.nix { };

          # fx3-mkimage is exposed rather than hidden inside the firmware derivation
          # because a contributor building the firmware outside Nix needs it on PATH too —
          # without it CMakeLists.txt falls back to compiling it from source each time.
          fx3-mkimage = pkgs.callPackage ./fx3/mkimage/package.nix { dddVersion = version; };
          fx3-firmware = pkgs.callPackage ./fx3/firmware/package.nix {
            inherit fx3-mkimage;
            firmwareVersion = version;
          };

          default = ddd-gui;
        }
        // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          fx3-programmer = pkgs.callPackage ./fx3/programmer/package.nix { };
        }
        // pkgs.lib.optionalAttrs (pkgs.stdenv.hostPlatform.system == quartusSystem) {
          bitstream = unfreePkgs.callPackage ./fpga/package.nix {
            quartus-prime-lite = quartus;
            bitstreamVersion = version;
          };

          # The toolchain itself, as an output, so that CI can name the thing it caches.
          # A workflow that wanted to warm a Quartus closure without this would have to
          # reach into the bitstream derivation's inputs and hope the attribute path
          # stayed put; naming it here makes the cache key a build definition rather than
          # a guess, and `nix build .#quartus-prime-lite` is also the shortest way for a
          # human to find out whether the Intel fetch still resolves.
          #
          # Exposing it redistributes nothing: this is a derivation that fetches from
          # Intel's CDN, not a copy of Quartus, and the closure cache the workflows use
          # is project-private for exactly that reason.
          quartus-prime-lite = quartus;
        }
      );

      devShells = forAllSystems (
        pkgs:
        {
          default = import ./nix/shell.nix { inherit pkgs; };
          ddd-gui = import ./ddd-gui/shell.nix { inherit pkgs; };
          fx3 = import ./fx3/shell.nix { inherit pkgs; };
          # Free tools only — lint and simulate the Verilog with no Quartus download.
          fpga = import ./fpga/shell.nix { inherit pkgs; };
          hardware = import ./hardware/shell.nix { inherit pkgs; };
          docs = import ./docs/shell.nix { inherit pkgs; };
        }
        // pkgs.lib.optionalAttrs (pkgs.stdenv.hostPlatform.system == quartusSystem) {
          # The full toolchain: everything in `fpga` plus Quartus itself. This shell,
          # rather than `nix build .#bitstream`, is the primary deliverable — the
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
      # no ctest suite and no package that CI can build, so its lint, style, simulation and
      # version checks are added here explicitly — they are the only automated coverage the
      # Verilog gets.
      # The licence-header and update-bundle checks belong to no component at all, so they
      # come from nix/: one is about every file in the tree, the other about the release
      # tooling in tools/, and neither has a component to live beside.
      #
      # The unfree outputs are removed rather than never added, so that a future package
      # added to that set cannot reach `checks` by being forgotten about here. Keep this
      # list in step with the `quartusSystem` block in `packages` above: `nix flake check`
      # is the tier every contributor runs, and it must not need an unfree download.
      checks = forAllSystems (
        pkgs:
        let
          fpgaChecks = pkgs.callPackage ./fpga/checks.nix { };
          repoChecks = pkgs.callPackage ./nix/checks.nix { src = self; };
        in
        builtins.removeAttrs self.packages.${pkgs.stdenv.hostPlatform.system} [
          "bitstream"
          "quartus-prime-lite"
          # The superseded capture application. Still a package, so it can be built on
          # purpose; not a check, so no contributor's `nix flake check` and no CI run
          # spends time building and testing an application nothing ships.
          "gui"
        ]
        // {
          fpga-lint = fpgaChecks.lint;
          fpga-style = fpgaChecks.style;
          fpga-sim = fpgaChecks.sim;
          fpga-sdc = fpgaChecks.sdc;
          fpga-provenance = fpgaChecks.provenance;
          fpga-version = fpgaChecks.version;
          fpga-boot-block = fpgaChecks.boot-block;
          fpga-halfband-coefficients = fpgaChecks.halfband-coefficients;
          licence-headers = repoChecks.licence-headers;
          update-bundle = repoChecks.update-bundle;
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
          domesday-duplicator-ddd-gui = final.qt6Packages.callPackage ./ddd-gui/package.nix {
            dddVersion = version;
            releaseUpdateKeyFile =
              if builtins.pathExists ./tools/keys/release.pub then ./tools/keys/release.pub else null;
          };
        }
        // final.lib.optionalAttrs final.stdenv.hostPlatform.isLinux {
          domesday-duplicator-fx3-programmer = final.callPackage ./fx3/programmer/package.nix { };
        };

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
