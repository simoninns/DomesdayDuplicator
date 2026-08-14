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
}:

let
  # The gateware, its testbenches, the lint and style configuration, and the
  # provenance tool. Deliberately not ./package.nix or ./shell.nix: a change to how
  # the bitstream is packaged should not invalidate the lint result.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./src
      ./tests
      ./verilator-waivers.vlt
      ./.verible-format
      ./.rules.verible_lint
      ./verible-waivers
      ./bitstream-provenance.py
      ./generate-version.sh
    ];
  };
in
{
  # T4 — lint. verilator --lint-only over the five project-authored modules.
  lint = runCommand "ddd-fpga-lint" { nativeBuildInputs = [ verilator ]; } ''
    bash ${src}/tests/run-lint.sh
    touch $out
  '';

  # T4 — style. verible-verilog-format --verify and verible-verilog-lint over the
  # five project-authored modules and the three testbenches.
  #
  # Separate from `lint` because the two answer different questions and fail for
  # different reasons: verilator asks whether the design is correct, verible asks
  # whether it is written the way this project writes Verilog. A style failure should
  # never be mistaken for a hardware bug, or the reverse.
  style = runCommand "ddd-fpga-style" { nativeBuildInputs = [ verible ]; } ''
    bash ${src}/tests/run-style.sh
    touch $out
  '';

  # T3 — simulation. The three module testbenches, under Icarus Verilog.
  sim = runCommand "ddd-fpga-sim" { nativeBuildInputs = [ iverilog ]; } ''
    bash ${src}/tests/run-sim.sh
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
}
