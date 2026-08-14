# Development shell for the FPGA gateware — **free tools only, no Quartus**.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#fpga
#
# There is a single flake.nix at the repository root and a single flake.lock beside it.
# Nix walks up to find them, so `.#fpga` resolves identically from `fpga/` and from the
# root. A bare `nix develop` gives the all-components default shell, not this one, whatever
# directory you happen to be in.
#
# Everything here is free software and cross-platform, so editing, linting and simulating
# the Verilog needs no Quartus download at all. Quartus is only required to produce a
# bitstream, and it lives in `nix develop .#fpga-quartus` (quartus-shell.nix), which is
# x86_64-linux only and a multi-gigabyte first download.
#
# verible-verilog-ls is a language server, so any editor with an LSP client gets completion,
# navigation and diagnostics in the Verilog sources.

{ pkgs }:

pkgs.mkShell {
  name = "ddd-fpga-hdl";

  packages = with pkgs; [
    # Lint and format. verible-verilog-ls is the language server.
    verible

    # Simulation. Both are used: verilator for linting and fast cycle-accurate models,
    # iverilog for event-driven testbenches with delays.
    verilator
    iverilog

    # Waveform viewer for the simulation output
    gtkwave
  ];

  shellHook = ''
    echo "Domesday Duplicator — FPGA gateware shell (free tools, no Quartus)"
    echo
    echo "  ./fpga/tests/run-lint.sh              lint the hand-written modules (T4)"
    echo "  ./fpga/tests/run-sim.sh               run the testbenches (T3)"
    echo "  ./fpga/tests/run-style.sh             check formatting and style (T4)"
    echo "  ./fpga/tests/run-format.sh            reformat the sources in place"
    echo "  ./fpga/tests/run-version.sh           check the version stamp generator (T2)"
    echo
    echo "Style settings live beside the sources and are what the checks read, so an"
    echo "editor, the dev shell and CI cannot disagree about them:"
    echo
    echo "  fpga/.verible-format                  formatter settings"
    echo "  fpga/.rules.verible_lint              style rules, and the departures from"
    echo "                                        Verible's defaults, each with a reason"
    echo "  fpga/verible-waivers                  narrow per-case exceptions"
    echo
    echo "verible-verilog-ls finds .rules.verible_lint on its own, so an LSP-capable"
    echo "editor shows the same diagnostics the CI check enforces."
    echo
    echo "IPfifo.v and IPpllGenerator.v instantiate Altera primitives with no free"
    echo "simulation model, so they are neither linted nor simulated — the black-box"
    echo "declarations beside them are what let the top level elaborate."
    echo
    echo "Bitstream builds need Quartus: nix develop .#fpga-quartus (x86_64-linux only)."
    echo
  '';
}
