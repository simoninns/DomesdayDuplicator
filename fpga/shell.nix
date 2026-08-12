# Development shell for the FPGA gateware — **free tools only, no Quartus**.
#
#   nix develop ./fpga       (or `nix develop .#fpga` from the repository root)
#
# Everything here is free software and cross-platform, so editing, linting and simulating
# the Verilog needs no Quartus download at all. Quartus is only required to produce a
# bitstream, and it arrives with the unfree shell added in Phase 6 (fpga/flake.nix), which
# is x86_64-linux only.
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
    echo "  verilator --lint-only -Wall src/dataGenerator.v      lint one module"
    echo "  verible-verilog-lint src/*.v                          style lint"
    echo "  verible-verilog-format --inplace src/statusLED.v      format"
    echo
    echo "IPfifo.v and IPpllGenerator.v instantiate Altera primitives and will not"
    echo "elaborate standalone — lint them, do not try to simulate them."
    echo
    echo "Bitstream builds need Quartus: nix develop ./fpga#quartus (x86_64-linux only)."
    echo
  '';
}
