# Checks for the FPGA component that need no Quartus.
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
  iverilog,
  python3,
}:

let
  # The gateware, its testbenches, the lint waivers and the provenance tool.
  # Deliberately not ./package.nix or ./shell.nix: a change to how the bitstream
  # is packaged should not invalidate the lint result.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./src
      ./tests
      ./verilator-waivers.vlt
      ./bitstream-provenance.py
    ];
  };
in
{
  # T4 — lint. verilator --lint-only over the five project-authored modules.
  lint = runCommand "ddd-fpga-lint" { nativeBuildInputs = [ verilator ]; } ''
    bash ${src}/tests/run-lint.sh
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
}
