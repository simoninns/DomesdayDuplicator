# Checks for the FPGA component that need no Quartus.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# These are the gateware's only automated coverage in CI. The bitstream build
# (package.nix) cannot run there — Quartus is unfree, x86_64-linux only and
# non-redistributable — so everything here is deliberately built on free,
# cross-platform tools that a runner can install from a binary cache.
#
# Each check runs the same script a developer runs in `nix develop .#fpga`,
# rather than reimplementing it in Nix, so the two cannot drift.

{
  lib,
  runCommand,
  verilator,
  verible,
  iverilog,
  python3,
  tcl,
}:

let
  # The gateware, its testbenches, the lint and style configuration, and the
  # provenance tool. Deliberately not ./package.nix or ./shell.nix: a change to how
  # the bitstream is packaged should not invalidate the lint result.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./application
      ./common
      ./factory
      ./tests
      ./verilator-waivers.vlt
      ./.verible-format
      ./.rules.verible_lint
      ./verible-waivers
      ./bitstream-provenance.py
      ./generate-version.sh
      ./make-boot-block.py
      ./make-halfband-coefficients.py
    ];
  };
in
{
  # T4 — lint. verilator --lint-only over the twelve project-authored modules,
  # across both images and the half they share.
  lint = runCommand "ddd-fpga-lint" { nativeBuildInputs = [ verilator ]; } ''
    bash ${src}/tests/run-lint.sh
    touch $out
  '';

  # T4 — style. verible-verilog-format --verify and verible-verilog-lint over
  # every project-authored module and testbench.
  #
  # Separate from `lint` because the two answer different questions and fail for
  # different reasons: verilator asks whether the design is correct, verible asks
  # whether it is written the way this project writes Verilog. A style failure should
  # never be mistaken for a hardware bug, or the reverse.
  style = runCommand "ddd-fpga-style" { nativeBuildInputs = [ verible ]; } ''
    bash ${src}/tests/run-style.sh
    touch $out
  '';

  # T3 — simulation. The module testbenches, under Icarus Verilog. The one
  # that matters most is tb_bootLoader: the factory image's boot decision is
  # the only logic here that a field update can never repair.
  sim = runCommand "ddd-fpga-sim" { nativeBuildInputs = [ iverilog ]; } ''
    bash ${src}/tests/run-sim.sh
    touch $out
  '';

  # T4 — timing constraints, for both images. The SDC is the only source file
  # Quartus alone consumes, and Quartus never runs in CI, so before this check
  # a mistyped constraint could sit in the tree until someone built a
  # bitstream. tclsh proves it parses; check-sdc.py proves it names every pin
  # the top level maps. Neither can say whether the numbers are right — that
  # needs Quartus.
  sdc = runCommand "ddd-fpga-sdc" { nativeBuildInputs = [ tcl python3 ]; } ''
    bash ${src}/tests/run-sdc.sh
    touch $out
  '';

  # T1/T2 — the byte offsets the canonical bitstream digest depends on.
  provenance = runCommand "ddd-fpga-provenance" { nativeBuildInputs = [ python3 ]; } ''
    python3 ${src}/tests/test_provenance.py
    touch $out
  '';

  # T2 — the commit-to-identity-register stamp the FX3 reads back over SPI.
  # iverilog is here to parse the generated file, not to simulate anything.
  version = runCommand "ddd-fpga-version" { nativeBuildInputs = [ iverilog ]; } ''
    bash ${src}/tests/run-version.sh
    touch $out
  '';

  # T1 — the boot block encoder. The twenty-four bytes it writes are read by
  # the factory image in fabric and, from Phase 5, by the update path on a
  # live device, so the format is asserted here by offset rather than by
  # asking the encoder what it thinks it wrote.
  boot-block = runCommand "ddd-fpga-boot-block" { nativeBuildInputs = [ python3 ]; } ''
    python3 ${src}/tests/test_boot_block.py
    touch $out
  '';

  # T1 — the decimation filter's coefficients. The table is committed into
  # halfBandDecimator.v because gateware cannot open a file, so this
  # regenerates it and fails if the two have parted company — and checks the
  # two properties the fabric is built around: a DC gain of exactly one, and a
  # centre tap of exactly half full scale, which is what lets that tap be a
  # shift rather than a seventeenth multiplier.
  halfband-coefficients =
    runCommand "ddd-fpga-halfband-coefficients" { nativeBuildInputs = [ python3 ]; } ''
      python3 ${src}/tests/test_halfband_coefficients.py
      touch $out
    '';
}
