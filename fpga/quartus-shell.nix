# Development shell for the FPGA gateware — **with Quartus**.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where to run it: anywhere in the working tree.
#
#   nix develop .#fpga-quartus
#
# x86_64-linux only. The nixpkgs quartus-prime-lite package is
# `platforms = [ "x86_64-linux" ]`, unfree and `redistributable = false`, so the
# attribute does not exist on any other system and the installer can never come
# from a binary cache — expect a multi-gigabyte download on first use. Restricted
# to the one device family this board needs, which removes five of the six
# component downloads.
#
# `nix develop .#fpga` is the same shell without Quartus: free tools only,
# cross-platform, and enough to edit, lint and simulate the Verilog. Use that
# one unless you are actually producing a bitstream.
#
# This shell is where gateware work is done. `nix build .#bitstream` makes a bitstream
# build repeatable and gives it a provenance record, but the GUI is never
# required for any step — compile, convert and program are all command-line
# tools, and they are all here.

{ pkgs, quartus }:

pkgs.mkShell {
  name = "ddd-fpga-quartus";

  packages = [
    quartus

    # The same free tooling as the Quartus-less shell, so switching between them
    # does not change what lint or simulation does.
    pkgs.verible
    pkgs.verilator
    pkgs.iverilog
    pkgs.gtkwave

    # bitstream-provenance.py
    pkgs.python3
  ];

  shellHook = ''
    echo "Domesday Duplicator — FPGA gateware shell (with Quartus)"
    echo
    echo "  $(quartus_sh --version 2>/dev/null | sed -n 2p)"
    echo
    echo "  ./fpga/build-local.sh                 compile + convert, out of tree"
    echo "  ./fpga/tests/run-lint.sh              lint the hand-written modules"
    echo "  ./fpga/tests/run-sim.sh               run the testbenches"
    echo
    echo "  quartus_pgm provisioning/DomesdayDuplicatorProvisioning_write_jic.cdf"
    echo "                                        permanent, both images, into the EPCS64"
    echo "  quartus_pgm application/DomesdayDuplicator_write_sof.cdf"
    echo "                                        volatile, lost on power cycle"
    echo
    echo "Do not run quartus_sh in fpga/application or fpga/factory: it rewrites"
    echo "the tracked .qsf in place"
    echo "to record LAST_QUARTUS_VERSION, and scatters build products beside the"
    echo "sources. build-local.sh copies to fpga/build first, which is what"
    echo "nix build .#bitstream does too."
    echo

    # Quartus writes settings and lock files into $HOME and will not start
    # without a writable one. An interactive shell has one; this is here so the
    # shell behaves the same when driven non-interactively.
    export HOME="''${HOME:-$TMPDIR}"
  '';
}
